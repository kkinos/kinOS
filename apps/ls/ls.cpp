#include <errno.h>
#include <string.h>

#include <cstdio>
#include <cstdlib>

#include "../../libs/common/message.hpp"
#include "../../libs/kinos/common/syscall.h"

int opendir(const char* name);
ssize_t readdir(int fd, void* buf);

extern "C" void main(int argc, char* argv[]) {
    const char* path = "/";
    char dirname[256];
    int fd;

    if (argc >= 2) {
        path = argv[1];
    }

    fd = opendir(path);
    if (fd == -1) {
        printf("unable to opendir %s\n", path);
        exit(1);
    }

    while (readdir(fd, dirname)) {
        printf("%s\n", dirname);
    }

    exit(0);
}

int opendir(const char* name) {
    struct SyscallResult id = SyscallFindServer("servers/am");
    if (id.error == 0) {
        struct Message smsg;
        struct Message rmsg;

        smsg.type = Message::kOpenDir;
        int i = 0;
        while (*name) {
            if (i > 25) {
                errno = EINVAL;
                return 0;
            }
            smsg.arg.opendir.dirname[i] = *name;
            ++i;
            ++name;
        }
        smsg.arg.opendir.dirname[i] = '\0';
        SyscallSendMessage(&smsg, id.value);

        while (1) {
            SyscallClosedReceiveMessage(&rmsg, 1, id.value);
            if (rmsg.type == Message::kError) {
                if (rmsg.arg.error.retry) {
                    SyscallSendMessage(&smsg, id.value);
                    continue;
                } else {
                    errno = rmsg.arg.error.err;
                    return -1;
                }
            } else if (rmsg.type == Message::kOpenDir) {
                break;
            }
        }
        return rmsg.arg.opendir.fd;
    }

    errno = id.error;
    return -1;
}

ssize_t readdir(int fd, void* buf) {
    struct SyscallResult id = SyscallFindServer("servers/am");
    if (id.error == 0) {
        struct Message smsg;
        struct Message rmsg;

        char* bufc = reinterpret_cast<char*>(buf);

        smsg.type = Message::kRead;
        smsg.arg.read.fd = fd;
        smsg.arg.read.count = 8 + 1;  // dir name length max is 8 + 1
        SyscallSendMessage(&smsg, id.value);

        size_t read_bytes = 0;
        while (1) {
            SyscallClosedReceiveMessage(&rmsg, 1, id.value);
            if (rmsg.type == Message::kError) {
                if (rmsg.arg.error.retry) {
                    SyscallSendMessage(&smsg, id.value);
                    continue;
                } else {
                    errno = rmsg.arg.error.err;
                    return 0;
                }
            } else if (rmsg.type == Message::kRead) {
                if (rmsg.arg.read.len) {
                    memcpy(&bufc[read_bytes], rmsg.arg.read.data, 9);
                    read_bytes += 8;
                } else {
                    break;
                }
            }
        }
        return read_bytes;
    } else {
        return 0;
    }
}