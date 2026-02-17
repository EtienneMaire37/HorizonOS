#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	tcsetpgrp(STDIN_FILENO, getpgrp());

    execvp("bash", (char*[]){"bash", (char*)NULL});

    perror("init: Couldn't run `bash`");
}
