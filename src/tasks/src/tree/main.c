// * basic clone of https://github.com/peteretelej/tree

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#define skip_option()   do { argv[i] = NULL; i++; } while (0)

void enumerate_dir(int depth, int maxdepth)
{
    if (depth >= maxdepth) return;
    DIR* dir = opendir(".");
    if (!dir)
    {
        perror("tree");
        return;
    }
    struct dirent* ent = readdir(dir);
    while (ent)
    {
        if (ent->d_name[0] != '.')
        {
            for (int i = 0; i < 4 * depth; i++)
                putchar((i % 4 == 0) ? '|' : ' ');
            printf("|-- %s\n", ent->d_name);
            if (ent->d_type == DT_DIR)
            {
                chdir(ent->d_name);
                enumerate_dir(depth + 1, maxdepth);
                fchdir(dirfd(dir));
            }
        }
        struct dirent* next = readdir(dir);
        ent = next;
    }
    closedir(dir);
}

int main(int argc, char** argv)
{
    DIR* cwd_dir = opendir(".");

    bool list_cwd = true;

    int L = INT_MAX;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (strcmp(argv[i], "-L") == 0)
            {
                skip_option();
                if (i >= argc)
                {
                    fprintf(stderr, "tree: Missing argument to -L option.\n");
                    return 1;
                }
                L = atoi(argv[i]);
                if (L <= 0)
                {
                    fprintf(stderr, "tree: Invalid level, must be greater than 0.\n");
                    return 1;
                }
            }
            else
            {
                fprintf(stderr, "tree: Unknown argument `%s`\n", argv[i]);
                return 1;
            }
            argv[i] = NULL;
        }
    }

    for (int i = 1; i < argc; i++)
    {
        if (!argv[i]) continue;

        list_cwd = false;

        if (chdir(argv[i]) == 0)
        {
            printf("%s\n", argv[i]);
            enumerate_dir(0, L);

            fchdir(dirfd(cwd_dir));
        }
        else
            printf("%s [error opening dir]\n", argv[i]);
    }

    if (list_cwd)
    {
        printf(".\n");
        enumerate_dir(0, L);
    }
}
