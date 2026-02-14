#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	tcsetpgrp(STDIN_FILENO, (pid_t)0x7fffffff);

    printf("ttyname: %s\n", ttyname(0));
    while (true);

    execvp("bash", (char*[]){"bash", (char*)NULL});

    perror("init: Couldn't run `bash`");
}
