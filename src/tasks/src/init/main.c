#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	tcsetpgrp(STDIN_FILENO, (pid_t)0x7fffffff);

    execvp("bash", (char*[]){"bash", (char*)NULL});

    perror("init: Couldn't run `bash`");
}
