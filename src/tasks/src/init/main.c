#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>

// void handler(int sig, siginfo_t* info, void* ucontext)
// {
//     printf("sig: %d\n", sig);
// }

int main(int argc, char** argv)
{
    tcsetpgrp(STDIN_FILENO, getpgrp());

    setenv("PATH", "/sbin:/bin:/usr/bin", 0);
    setenv("HOME", "/root", 0);

    setenv("TERM", "hterm", 0);
    setenv("TERMINFO", "/usr/share/terminfo", 0);

    chdir(getenv("HOME"));

    // struct sigaction sg;
    // sg.sa_flags = SA_SIGINFO;
    // sg.sa_sigaction = handler;
    // sigaction(SIGINT, &sg, NULL);

    // while (1)
    // {
    //     // putchar('A');
    // }

    execvp("bash", (char*[]){"bash", (char*)NULL});

    perror("init: Couldn't run `bash`");
}
