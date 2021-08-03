#include "../../libs/kinos/syscall.h"
#include <stdlib.h>

int kexec (char* command_line) {

    SyscallRestartTask(command_line);
    exit(0);
    return -1;
}

extern "C" void main(int argc, char *argv[]) {
    kexec("apps/cube");

}