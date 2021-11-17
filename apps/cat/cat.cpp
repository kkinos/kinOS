#include <cstdio>
#include <cstdlib>

extern "C" void main(int argc, char** argv) {
    const char* path;
    if (argc == 1) {
        printf("Usage: %s <file>\n", argv[0]);
        exit(1);
    }
    if (argc >= 2) {
        path = argv[1];
    }

    FILE* fp = fopen(path, "r");
    if (fp == nullptr) {
        printf("failed to open: %s\n", path);
        exit(1);
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }
    exit(0);
}