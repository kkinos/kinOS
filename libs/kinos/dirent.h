#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int fd;
} DIR;

struct dirent {
    char d_name[256];
};

DIR* opendir(const char* name);

struct dirent* readdir(DIR* dir);

#ifdef __cplusplus
}  // extern "C"
#endif