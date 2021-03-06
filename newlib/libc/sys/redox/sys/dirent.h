#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H

struct dirent {
    char d_name[4096];
};

typedef struct __dirstream DIR;


#define __dirfd(dir) (dir)->dd_fd

/* --- redundant --- */

DIR *opendir(const char *);
struct dirent *readdir(DIR *);
void rewinddir(DIR *);
int closedir(DIR *);

#endif
