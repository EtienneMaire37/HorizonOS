#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>

int main(int argc, char** argv)
{
    assert(open("/dev/tty", O_RDONLY) == STDIN_FILENO);
    assert(open("/dev/tty", O_WRONLY) == STDOUT_FILENO);
    assert(open("/dev/tty", O_WRONLY) == STDERR_FILENO);

    setenv("PATH", "/sbin:/bin:/usr/bin", 0);
    setenv("HOME", "/root", 0);

    tcsetpgrp(STDIN_FILENO, getpgrp());

    execvp("dash", (char*[]){"dash", (char*)NULL});

    perror("init: Couldn't run `dash`");
}