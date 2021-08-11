#include <cstdlib>

#include "../../libs/kinos/syscall.h"

extern "C" int main() {
    for (int i = 0; i < 100; ++i) {
        for (int j = 0; j < 100; ++j) {
            SyscallWritePixel(i, j, 255, 255, 255);
        }
    }
    exit(0);
    
}