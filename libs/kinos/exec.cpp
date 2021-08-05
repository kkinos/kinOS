#include "exec.hpp"
#include "syscall.h"

int kexec (char* command_line) {

    SyscallRestartTask(command_line);
    exit(1);
    return -1;
}

int kexec2 (char* command_line) {
    struct SyscallResult res = SyscallCreateAppTask(command_line);
    if (res.error) {
        return -1;
    }    
    return res.value;
    
}

