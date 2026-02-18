#include <unistd.h>
#include <stdio.h>

#include <signal.h>

int i = 0;

void sighandler(int sig)
{
    i++;
    i %= 'Z' - 'A' + 1;
}

int main(int argc, char** argv)
{
	tcsetpgrp(STDIN_FILENO, getpgrp());

    struct sigaction act;
    act.sa_flags = SA_RESTART;
    act.sa_handler = sighandler;
    sigaction(SIGINT, &act, NULL);

    while (1)
        putchar('A' + i);

    execvp("bash", (char*[]){"bash", (char*)NULL});

    perror("init: Couldn't run `bash`");
}
