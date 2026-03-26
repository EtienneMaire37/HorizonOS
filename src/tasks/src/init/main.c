#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/ioctl.h>

int main(int argc, char** argv)
{
    tcsetpgrp(STDIN_FILENO, getpgrp());

    setenv("PATH", "/sbin:/bin:/usr/bin", 0);
    setenv("HOME", "/root", 0);

    setenv("TERM", "hterm", 0);
    setenv("TERMINFO", "/usr/share/terminfo", 0);

    chdir(getenv("HOME"));

    struct winsize size;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &size);

    char row[64];
    char col[64];
    snprintf(row, sizeof(row), "%d", size.ws_row);
    snprintf(col, sizeof(col), "%d", size.ws_col);

    setenv("LINES", row, 0);
    setenv("COLUMNS", col, 0);

    execvp("bash", (char*[]){"bash", (char*)NULL});

    perror("init: Couldn't run `bash`");
}
