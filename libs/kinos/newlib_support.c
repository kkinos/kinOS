#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "syscall.h"

int close(int fd) {
    errno = EBADF;
    return -1;
}

int fstat(int fd, struct stat* buf) {
    errno = EBADF;
    return -1;
}

pid_t getpid(void) { return 0; }

int isatty(int fd) {
    errno = EBADF;
    return -1;
}

int kill(pid_t pid, int sig) {
    errno = EPERM;
    return -1;
}

off_t lseek(int fd, off_t offset, int whence) {
    errno = EBADF;
    return -1;
}

int open(const char* path, int flags) {
    struct SyscallResult id = SyscallFindServer("servers/am");
    if (id.error == 0) {
        struct Message smsg;
        struct Message rmsg;

        smsg.type = kOpen;
        int i = 0;
        while (*path) {
            if (i > 25) {
                errno = EINVAL;
                return -1;
            }
            smsg.arg.open.filename[i] = *path;
            ++i;
            ++path;
        }
        smsg.arg.open.filename[i] = '\0';
        smsg.arg.open.flags = flags;

        SyscallSendMessage(&smsg, id.value);

        while (1) {
            SyscallClosedReceiveMessage(&rmsg, 1, id.value);
            if (rmsg.type == kError) {
                if (rmsg.arg.error.retry) {
                    SyscallSendMessage(&smsg, id.value);
                    continue;
                } else {
                    errno = EAGAIN;
                    return -1;
                }
            } else if (rmsg.type == kOpen) {
                break;
            }
        }

        if (!rmsg.arg.open.exist) {
            errno = ENOENT;
            return -1;
        }
        return rmsg.arg.open.fd;
    }

    errno = id.error;
    return -1;
}

int posix_memalign(void** memptr, size_t alignment, size_t size) {
    void* p = malloc(size + alignment - 1);
    if (!p) {
        return ENOMEM;
    }
    uintptr_t addr = (uintptr_t)p;
    *memptr = (void*)((addr + alignment - 1) & ~(uintptr_t)(alignment - 1));
    return 0;
}

ssize_t read(int fd, void* buf, size_t count) {
    struct SyscallResult id = SyscallFindServer("servers/am");
    if (id.error == 0) {
        struct Message smsg;
        struct Message rmsg;

        smsg.type = kRead;
        smsg.arg.read.fd = fd;
        smsg.arg.read.count = count;
        SyscallSendMessage(&smsg, id.value);

        const char* bufc = (const char*)buf;
        size_t read_bytes = 0;
        while (1) {
            SyscallClosedReceiveMessage(&rmsg, 1, id.value);
            if (rmsg.type == kError) {
                if (rmsg.arg.error.retry) {
                    SyscallSendMessage(&smsg, id.value);
                    continue;
                } else {
                    errno = EAGAIN;
                    return -1;
                }
            } else if (rmsg.type == kRead) {
                if (rmsg.arg.read.len) {
                    memcpy(&bufc[read_bytes], rmsg.arg.read.data,
                           rmsg.arg.read.len);
                    read_bytes += rmsg.arg.read.len;
                } else {
                    break;
                }
            }
        }
        return read_bytes;
    }

    errno = id.error;
    return -1;
}

caddr_t sbrk(int incr) {
    static uint64_t dpage_end = 0;
    static uint64_t program_break = 0;

    if (dpage_end == 0 || dpage_end < program_break + incr) {
        int num_pages = (incr + 4095) / 4096;
        struct SyscallResult res = SyscallDemandPages(num_pages, 0);
        if (res.error) {
            errno = ENOMEM;
            return (caddr_t)-1;
        }
        program_break = res.value;
        dpage_end = res.value + 4096 * num_pages;
    }

    const uint64_t prev_break = program_break;
    program_break += incr;
    return (caddr_t)prev_break;
}

ssize_t write(int fd, const void* buf, size_t count) {
    struct SyscallResult id = SyscallFindServer("servers/am");
    if (id.error == 0) {
        struct Message smsg;
        struct Message rmsg;

        smsg.type = kWrite;
        smsg.arg.write.fd = fd;
        const char* bufc = (const char*)buf;
        size_t sent_bytes = 0;
        while (sent_bytes < count) {
            if (count - sent_bytes > sizeof(smsg.arg.write.data)) {
                smsg.arg.write.len = sizeof(smsg.arg.write.data);
            } else {
                smsg.arg.write.len = count - sent_bytes;
            }
            memcpy(smsg.arg.write.data, &bufc[sent_bytes], smsg.arg.write.len);
            sent_bytes += smsg.arg.write.len;
            SyscallSendMessage(&smsg, id.value);

            while (1) {
                SyscallClosedReceiveMessage(&rmsg, 1, id.value);
                if (rmsg.type == kError) {
                    if (rmsg.arg.error.retry) {
                        SyscallSendMessage(&smsg, id.value);
                        continue;
                    }
                } else if (rmsg.type == kReceived) {
                    break;
                }
            }
        }
        return count;
    }

    errno = id.error;
    return -1;
}

void _exit(int status) { SyscallExit(status); }
