#include "fork.hpp"
#include "syscall.h"

int kfork(void) {
  struct SyscallResult res = SyscallCloneTask();
    if (res.error) {
        return -1;
    }    
    return res.value;
}