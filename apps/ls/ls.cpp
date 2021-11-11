
#include <string.h>

#include <cstdio>
#include <cstdlib>

#include "../../libs/kinos/dirent.h"

int main(int argc, char *argv[]) {
    DIR *dir;
    struct dirent *ent;
    const char *path = "apps/";
    char dirname[256];

    if (argc >= 2) {
        path = argv[1];
    }

    dir = opendir(path);
    if (dir == nullptr) {
        printf("unable to opendir %s\n", dirname);
        exit(1);
    }
    printf("%d\n", dir->fd);

    // while ((ent = readdir(dir)) != NULL) {
    //     printf("%s\n", ent->d_name);
    // }
    exit(0);
}