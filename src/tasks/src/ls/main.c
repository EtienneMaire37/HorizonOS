#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../../libc/src/misc.h"

extern int errno;

int main(int argc, char** argv)
{
    bool l = false, a = false;

    int num_paths = 0;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            num_paths++;
            continue;
        }

        for (char* c = argv[i] + 1; *c; c++)
        {
            switch (*c)
            {
            case 'l':
                l = true;
                break;
            case 'a':
                a = true;
                break;
            default:
                fprintf(stderr, "ls: invalid argument \'%c\'\n", *c);
                return 2;
            }
        }
    }

    int paths_seen = 0;

    char* path = "./";
    if (num_paths == 0)
        goto ls_path;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
            continue;

        path = argv[i];
        if (num_paths > 1)
            printf("%s:\n", path);
        paths_seen++;
    ls_path:
        DIR* dp;
        struct dirent* ep;
        dp = opendir(path);
        if (dp != NULL)
        {
            while ((ep = readdir(dp)) != NULL)
            {
                bool hidden = ep->d_name[0] == '.';
                if (!a && hidden)
                    continue;
                char full_path[PATH_MAX];
                snprintf(full_path, sizeof(full_path), "%s/%s", path, ep->d_name);
                struct stat st;
                if (stat(full_path, &st) != 0)
                    continue;
                bool dir = S_ISDIR(st.st_mode);
                if (l)
                {
                    char access_str[11];

                    uint8_t magnitude = 0;
                    size_t magnitude_value = (size_t)st.st_size;
                    const char* magnitude_text[] = {"B ", "KB", "MB", "GB", "TB", "PB", "EB"};
                    while (magnitude_value >= 1024 * 1024)
                    {
                        magnitude_value /= 1024;
                        magnitude++;
                    }
                    if (magnitude_value >= 1024)
                        magnitude++;
                    if (magnitude == 0)
                        magnitude_value *= 1024;

                    printf("%s % 4zu", get_access_string(st.st_mode, access_str), magnitude_value / 1024);
                    if (magnitude != 0)
                        printf(".%zu%zu", (magnitude_value * 10 / 1024) % 10, (magnitude_value * 100 / 1024) % 10);
                    printf("%s ", magnitude_text[magnitude]);
                    if (magnitude == 0)
                        printf("   ");
                }
                if (dir)
                {
                    if (hidden)
                        printf("\x1b[94m");
                    else
                        printf("\x1b[93m");
                }
                else
                {
                    if (hidden)
                        printf("\x1b[95m");
                    else
                        printf("\x1b[92m");
                }
                printf("%s\x1b[0m\n", ep->d_name);
            }

            closedir(dp);
        }
        else
        {
            int _errno = errno;
            char buffer[256] = {0};
            snprintf(buffer, 256, "ls: cannot access \'%s\'", path);
            errno = _errno;
            perror(buffer);
        }

        if (num_paths == 0)
            return 0;

        if (paths_seen != num_paths)
            putchar('\n');
    }
}