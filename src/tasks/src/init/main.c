#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>

int main(int argc, char** argv)
{
    tcsetpgrp(STDIN_FILENO, getpgrp());

    setenv("PATH", "/sbin:/bin:/usr/bin", 0);
    setenv("HOME", "/root", 0);

    chdir(getenv("HOME"));

    execvp("bash", (char*[]){"bash", (char*)NULL});

    perror("init: Couldn't run `bash`");
}