#include "../../libs/kinos/syscall.h"
#include <sys/types.h> 
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int kexec (char* command_line) {

    SyscallRestartTask(command_line);
    exit(1);
    return -1;
}

extern "C" void main(int argc, char *argv[]) {
    int ret;
    ret = fork();
    if (ret == 0) {
        kexec("apps/cube");
    } else {
        exit(0);
    }
}