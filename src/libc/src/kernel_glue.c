#pragma once

ssize_t write(int fildes, const void* buf, size_t nbyte)
{
    ssize_t bytes_written;
    int _errno;
    asm volatile("int 0xf0" : "=a" (bytes_written), "=b" (_errno)
        : "a" (SYSCALL_WRITE), "b" (fildes), "c" (buf), "d" (nbyte));
    if (_errno) errno = _errno;
    return bytes_written;
}

ssize_t read(int fildes, void* buf, size_t nbyte)
{
    ssize_t bytes_read;
    int _errno;
    asm volatile("int 0xf0" : "=a" (bytes_read), "=b" (_errno)
        : "a" (SYSCALL_READ), "b" (fildes), "c" (buf), "d" (nbyte) : "memory");
    if (_errno) errno = _errno;
    return bytes_read;
}

void exit(int r)
{
    fflush(stdout);

    for (uint8_t i = 0; i < atexit_stack_length; i++)
        if (atexit_stack[atexit_stack_length - i - 1] != NULL)
            atexit_stack[atexit_stack_length - i - 1]();

    asm volatile("int 0xf0" : 
        : "a" (SYSCALL_EXIT), "b" (r));

    while (true);
    __builtin_unreachable();
}

time_t time(time_t* t)
{
    time_t now = 0;
    asm volatile("int 0xf0" : "=a" (now)
        : "a" (SYSCALL_TIME));
    if (t) *t = now;
    return now;
}

pid_t getpid()
{
    pid_t ret;
    asm volatile("int 0xf0" : "=a" (ret) : "a" (SYSCALL_GETPID));
    return ret;
}

pid_t fork()
{
    pid_t ret;
    asm volatile("int 0xf0" : "=a" (ret) : "a" (SYSCALL_FORK));
    return ret;
}

int brk(void* addr)
{
    if ((uint64_t)addr == break_address) return 0;
    if ((uint64_t)addr < heap_address) 
    {
        errno = ENOMEM;
        return -1;
    }
    int _errno;
    asm volatile("int 0xf0" : "=a" (_errno), "=b"(break_address)
                : "a" (SYSCALL_BRK), "b" (addr), "c"(break_address));
    if (_errno)
    {
        errno = _errno;
        return -1;
    }
    return 0;
}

int open(const char* path, int oflag, ...)
{
    mode_t mode = 0;
    if ((oflag & O_CREAT) || (oflag & O_TMPFILE))
    {
        va_list args;
        va_start(args, oflag);
        mode = va_arg(args, mode_t) & ~fd_creation_mask;
        va_end(args);
    }
    int fd, _errno;
    asm volatile ("int 0xf0" : "=a"(_errno), "=b"(fd) : "a"(SYSCALL_OPEN), "b"(path), "c"(oflag), "d"(mode));
    if (_errno != 0)
        errno = _errno;
    return fd;
}

int close(int fildes)
{
    int ret, _errno;
    asm volatile ("int 0xf0" : "=a"(_errno), "=b"(ret) : "a"(SYSCALL_CLOSE), "b"(fildes));
    if (_errno != 0)
        errno = _errno;
    return ret;
}

int execve(const char* path, char* const argv[], char* const envp[])
{
    int _errno;
    asm volatile ("int 0xf0" : "=a"(_errno) : "a"(SYSCALL_EXECVE), "b"(path), "c"(argv), "d"(envp));
    if (_errno != 0)
        errno = _errno;
    return -1;
}

pid_t waitpid(pid_t pid, int* wstatus, int options)
{
    int _wstatus, _errno;
    pid_t ret;
    asm volatile ("int 0xf0" : "=a"(_errno), "=b"(_wstatus), "=c"(ret) : "a"(SYSCALL_WAITPID), "b"(pid), "d"(options) : "memory");
    if (wstatus) *wstatus = _wstatus;
    if (_errno != 0)
        errno = _errno;
    return ret;
}

int access(const char* path, int mode)
{
    int ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_ACCESS), "b"(path), "c"(mode));
    if (ret != 0)
    {
        errno = ret;
        return -1;
    }
    return 0;
}

int stat(const char* path, struct stat* statbuf)
{
    int ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_STAT), "b"(path), "c"((uint64_t)statbuf) : "memory");
    if (ret != 0)
    {
        errno = ret;
        return -1;
    }
    return 0;
}

static struct dirent dirent_entry;      // * "This structure may be statically allocated"

struct dirent* readdir(DIR* dirp)
{
    uint64_t return_address;
    int _errno;
    asm volatile ("int 0xf0" : "=a"(_errno), "=b"(return_address) : "a"(SYSCALL_READDIR), "b"((uint64_t)&dirent_entry), "c"((uint64_t)dirp) : "memory");
    if (_errno != 0)
        errno = _errno;
    return (struct dirent*)return_address;
}

int isatty(int fd)
{
    int ret, _errno;
    asm volatile ("int 0xf0" : "=a"(_errno), "=b"(ret) : "a"(SYSCALL_ISATTY), "b"(fd));
    if (_errno != 0)
        errno = _errno;
    return ret;
}

int tcgetattr(int fildes, struct termios* termios_p)
{
    int ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_TCGETATTR), "b"(fildes), "c"(termios_p));
    if (ret)
    {
        errno = ret;
        return -1;
    }
    return 0;
}

int tcsetattr(int fildes, int optional_actions, const struct termios* termios_p)
{
    int ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_TCSETATTR), "b"(fildes), "c"(termios_p), "d"(optional_actions));
    if (ret)
    {
        errno = ret;
        return -1;
    }
    return 0;
}

int chdir(const char* path)
{
    if (!path) 
    {
        errno = EFAULT;
        return -1;
    }

    int ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_CHDIR), "b"(path));
    if (ret)
    {
        errno = ret;
        return -1;
    }
    return 0;
}

char* getcwd(char* buffer, size_t size)
{
    if (!buffer) return NULL;
    char* ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_GETCWD), "b"(buffer), "c"(size));
    return ret;
}

char* realpath(const char* path, char* resolved_path)
{
    if (!path) 
    {
        errno = EINVAL;
        return NULL;
    }

    if (!resolved_path) resolved_path = malloc(PATH_MAX * sizeof(char));
    if (!resolved_path)
    {
        errno = ENOMEM;
        return NULL;
    }

    int ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_REALPATH), "b"(path), "c"(resolved_path));
    if (ret)
    {
        errno = ret;
        return NULL;
    }
    return resolved_path;
}

off_t lseek(int fd, off_t offset, int whence)
{
    int ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_LSEEK), "b"(fd), "c"((uint64_t)offset), "d"(whence));
    if (ret < 0)
    {
        errno = -ret;
        return -1;
    }
    return ret;
}

pid_t tcgetpgrp(int fd)
{
    pid_t ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_TCGETPGRP), "b"(fd));
    if (ret < 0)
    {
        errno = -ret;
        return -1;
    }
    return ret;
}

int tcsetpgrp(int fd, pid_t pgrp)
{
    int ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_TCSETPGRP), "b"(fd), "c"(pgrp));
    if (ret)
    {
        errno = ret;
        return -1;
    }
    return 0;
}

pid_t getpgid(pid_t pid)
{
    pid_t ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_GETPGID), "b"(pid));
    if (ret < 0)
    {
        errno = -ret;
        return -1;
    }
    return ret;
}

int setpgid(pid_t pid, pid_t pgid)
{
    pid_t ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_SETPGID), "b"(pid), "c"(pgid));
    if (ret)
    {
        errno = ret;
        return -1;
    }
    return 0;
}

int fstat(int fd, struct stat* buf)
{
    int ret;
    asm volatile ("int 0xf0" : "=a"(ret) : "a"(SYSCALL_FSTAT), "b"(fd), "c"((uint64_t)buf) : "memory");
    if (ret != 0)
    {
        errno = ret;
        return -1;
    }
    return 0;
}