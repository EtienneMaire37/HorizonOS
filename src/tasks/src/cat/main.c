#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    char data[BUFSIZ];
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
        if (!fd)
        {
            int _errno = errno;
            fprintf(stderr, "cat: %s: ", argv[i]);
            errno = _errno;
            perror("");
            continue;
        }

        size_t bytes_read;
        while ((bytes_read = read(fd, data, BUFSIZ)) > 0)
            fwrite(data, 1, bytes_read, stdout);

        close(fd);
    }
}