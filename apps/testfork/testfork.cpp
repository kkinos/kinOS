#include <sys/types.h> 
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "../syscall.h"

int kfork() {    
    auto [task_id, err]
        = SyscallTaskClone();
    if (err) {
        return -1;
    }    
    return task_id;
}

extern "C" void main(int argc, char *argv[]) {

    int ret;

    printf("start!\n");
    ret = kfork();

    if(ret  == -1){
        printf("fork() failed.");
        exit(0);
    }
    if(ret == 0) {
        printf("child\n");
        printf("end!\n");
        exit(1);

    }
    printf("parent\n");
    printf("end!\n");

    exit(1);
}