#include <sys/types.h> 
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../libs/kinos/fork.hpp"
#include "../../libs/kinos/exec.hpp"

extern "C" void main(int argc, char *argv[]) {
    int ret;
    ret = kfork();
    if (ret == 0) {
        kexec("apps/cube");
    } else {
        exit(0);
    }
}