#pragma once

#include "../../kernel/debug/out.h"
#include "../../kernel/cpu/util.h"
#include "../../kernel/vga/textio.h"

ssize_t write(int fildes, const void* buf, size_t nbyte)
{
    // abort();

    if (fildes > 2)
    {
        LOG(ERROR, "Invalid output stream");
        errno = EBADF;
        return -1;
    }

    switch(fildes)
    {
    case STDIN_FILENO: 
        return 0;
    case STDERR_FILENO: // * Kernel writes to stderr -> log
        for (size_t i = 0; i < nbyte; i++)
            debug_outc(*((char*)buf + i));
        return nbyte;
    case STDOUT_FILENO:
        for (size_t i = 0; i < nbyte; i++)
            tty_outc(*((char*)buf + i));
        return nbyte;
    }

    return 0;
}

ssize_t read(int fildes, void* buf, size_t nbyte)
{
    abort();
}

void exit(int r)
{
    disable_interrupts();
    LOG(CRITICAL, "Kernel exited with return code %d", r);
    tty_set_color(FG_WHITE, BG_BLACK);
    printf("\nKernel exited with return code ");
    tty_set_color(FG_LIGHTGREEN, BG_BLACK);
    printf("%d", r);
    tty_set_color(FG_WHITE, BG_BLACK);
    printf(".");
    fflush(stdout);
    halt();
    while (true);
    __builtin_unreachable();
}

time_t time(time_t* t)
{
    abort();
}

pid_t getpid()
{
    abort();
}

pid_t fork()
{
    abort();
}

int brk(void*addr)
{
    abort();
}

int open(const char* path, int oflag, ...)
{
    abort();
}

int close(int fildes)
{
    abort();
}

int execve(const char* path, char* const argv[], char* const envp[])
{
    abort();
}

pid_t waitpid(pid_t pid, int* wstatus, int options)
{
    abort();
}

int access(const char* path, int mode)
{
    abort();
}

int stat(const char* path, struct stat* statbuf)
{
    abort();
}

int isatty(int fd)
{
    abort();
}

int tcgetattr(int fildes, struct termios* termios_p)
{
    abort();
}

int tcsetattr(int fildes, int optional_actions, const struct termios* termios_p)
{
    abort();
}

int chdir(const char* path)
{
    abort();
}

char* getcwd(char* buffer, size_t size)
{
    abort();
}

char* realpath(const char* path, char* resolved_path)
{
    abort();
}

off_t lseek(int fd, off_t offset, int whence)
{
    abort();
}

pid_t tcgetpgrp(int fd)
{
    abort();
}

int tcsetpgrp(int fd, pid_t pgrp)
{
    abort();
}

pid_t getpgid(pid_t pid)
{
    abort();
}

int setpgid(pid_t pid, pid_t pgid)
{
    abort();
}

int fstat(int fd, struct stat* buf)
{
    abort();
}