#include "kexec.hpp"
#include "syscall.h"

int kexec (char* command_line) {

    SyscallRestartTask(command_line);
    exit(1);
    return -1;
}