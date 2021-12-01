#include <cstdio>
#include <cstdlib>

extern "C" void main(int argc, char** argv) {
    --argc;

    for (int i = 1; i <= argc; ++i) {
        printf("%s ", argv[i]);
    }
    printf("\n");
    exit(0);
}