#pragma once

#include <inttypes.h>
#include <sys/types.h>

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

#define F_OK    1
#define R_OK    2
#define W_OK    4
#define X_OK    8

#include "seek_defs.h"

pid_t getpid();
pid_t fork();
ssize_t write(int fildes, const void* buf, size_t nbyte);
ssize_t read(int fildes, void* buf, size_t nbyte);
int close(int fildes);
int brk(void* addr);
void* sbrk(intptr_t incr);
off_t lseek(int fildes, off_t offset, int whence);
int gethostname(char* name, size_t namelen);
int chdir(const char* path);
char* getcwd(char* buffer, size_t size);
int execve(const char* path, char* const argv[], char* const envp[]);
int execv(const char* path, char* const argv[]);
int execvpe(const char* file, char* const argv[], char* const envp[]);
int execvp(const char* file, char* const argv[]);
int access(const char* path, int mode);
int isatty(int fd);
off_t lseek(int fd, off_t offset, int whence);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
int setpgid(pid_t pid, pid_t pgid);
pid_t getpgid(pid_t pid);
pid_t getpgrp();