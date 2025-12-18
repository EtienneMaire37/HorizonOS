#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void do_cat(int fd)
{
    char data[BUFSIZ];

    size_t bytes_read;
    while ((bytes_read = read(fd, data, BUFSIZ)) > 0)
        fwrite(data, 1, bytes_read, stdout);
}

int main(int argc, char** argv)
{
    if (argc == 1) // * Use stdin
    {
        struct stat st;
        if (fstat(STDIN_FILENO, &st) == 0 && S_ISDIR(st.st_mode))
        {
            fprintf(stderr, "cat: -: ");
            errno = EISDIR;
            perror("");
            return 1;
        }
        do_cat(STDIN_FILENO);
        return 0;
    }

    for (int i = 1; i < argc; i++)
    {
        struct stat st;
        if (stat(argv[i], &st) == 0 && S_ISDIR(st.st_mode))
        {
            fprintf(stderr, "cat: %s: ", argv[i]);
            errno = EISDIR;
            perror("");
            continue;
        }

        int fd = open(argv[i], O_RDONLY);

        if (fd == -1)
        {
            int _errno = errno;
            fprintf(stderr, "cat: %s: ", argv[i]);
            errno = _errno;
            perror("");
            continue;
        }
        
        do_cat(fd);

        close(fd);
    }
}