#include <sys/types.h> 
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "../../libs/kinos/fork.hpp"

extern "C" void main(int argc, char *argv[]) {

    int ret;
    ret = kfork();

    if(ret  == -1){
        printf("\nfork() failed.\n");
        exit(0);
    } else if (ret == 0) {
        printf("\nIm child\n");
        
        exit(1);
    } else {
        printf("\nIm parent\n");
        printf("\nchild task id is %d\n", ret);
        exit(1);
    }

}