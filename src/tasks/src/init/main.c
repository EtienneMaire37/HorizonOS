#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	tcsetpgrp(STDIN_FILENO, (pid_t)0x7fffffff);

    execvp("term", (char*[]){"term", (char*)NULL});

    perror("init: Couldn't run `term`");
}
