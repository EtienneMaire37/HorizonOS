#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <sys/wait.h>
#include <termios.h>
#include <sys/types.h>
#include <fcntl.h>

static char* find_next_contiguous_string(char* str, int* bytes_left)
{
    if (!str) return NULL;
    if (!bytes_left) return NULL;
    while (*str && (*bytes_left) > 0)
    {
        str++;
        (*bytes_left)--;
    }
    while (!(*str) && (*bytes_left) > 0)
    {
        str++;
        (*bytes_left)--;
    }
    if ((*bytes_left) <= 0) return NULL;
    return str;
}

static char* find_first_arg(char* data, int* bytes_left)
{
    return data[0] ? data : find_next_contiguous_string(data, bytes_left);
}

const char* kb_layouts[] = {"us_qwerty", "fr_azerty"};

#define HISTORY_MAX 128

struct termios oldt, newt;

void set_raw_mode(bool enable) 
{
    if (enable) 
    {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO | ISIG);   // * Disable canonical mode and echoing and interrupts
        newt.c_cc[VMIN] = 1;
        newt.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } 
    else
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

void disable_raw_mode_and_exit()
{
    set_raw_mode(false);
    exit(0);
}

int main(int argc, char** argv)
{
    set_raw_mode(true);

    setenv("?", "0", true);

    char cwd[PATH_MAX];
    char* command_history[HISTORY_MAX] = {NULL};

    while (true)
    {
        getcwd(cwd, sizeof(cwd));
        printf("\x1b[36m%s\x1b[0m$ ", cwd);
        fflush(stdout);
        char data[BUFSIZ] = {0};
        char byte;
        int write_position = 0;
        int history_index = -1;
        while (read(STDIN_FILENO, &byte, 1) > 0)
        {
            if (byte == '\n' || byte == newt.c_cc[VEOF] || write_position >= BUFSIZ - 1)
                break;
            else
            {
                if (byte == newt.c_cc[VERASE])
                {
                    if (write_position > 0)
                    {
                        write_position--;
                        printf("\b \b");
                    }
                }
                else
                switch (byte)
                {
                case '\x1b':    // * escape sequence
                {
                    char next_bytes[8];
                    // * VMIN == 1 so it should be safe
                    read(STDIN_FILENO, &next_bytes[0], 1);
                    read(STDIN_FILENO, &next_bytes[1], 1);
                    int byte_count = 1;
                    while (next_bytes[byte_count] >= '0' && next_bytes[byte_count] <= '9' && byte_count < 7)
                        read(STDIN_FILENO, &next_bytes[++byte_count], 1);
                    if (next_bytes[0] == '[')
                    {
                        switch (next_bytes[1])
                        {
                        case 'A':   // * Up arrow
                            if (history_index < HISTORY_MAX - 1)
                            {
                                if (command_history[history_index + 1] != NULL)
                                {
                                    strncpy(data, command_history[history_index + 1], BUFSIZ);
                                    data[BUFSIZ - 1] = 0;
                                    for (int i = 0; i < write_position; i++)
                                        printf("\b \b");
                                    write_position = strlen(data);
                                    for (int i = 0; i < write_position; i++)
                                        putchar(data[i]);
                                    history_index++;
                                }
                            }
                            break;
                        case 'B':   // * Down arrow
                            if (history_index > 0)
                            {
                                if (command_history[history_index - 1] != NULL)
                                {
                                    strncpy(data, command_history[history_index - 1], BUFSIZ);
                                    data[BUFSIZ - 1] = 0;
                                    for (int i = 0; i < write_position; i++)
                                        printf("\b \b");
                                    write_position = strlen(data);
                                    for (int i = 0; i < write_position; i++)
                                        putchar(data[i]);
                                    history_index--;
                                }
                            }
                            else if (history_index == 0)
                            {
                                for (int i = 0; i < write_position; i++)
                                    printf("\b \b");
                                data[0] = 0;
                                write_position = 0;
                                history_index--;
                            }
                            break;
                        default:
                            ;
                        }
                    }
                    break;
                }
                default:
                    data[write_position++] = byte;
                    putchar(byte);
                }
            }
            fflush(stdout);
        }
        data[write_position] = 0;
        putchar('\n');

        if (strcmp(data, "") != 0)
        {
            free(command_history[HISTORY_MAX - 1]);
            for (int i = HISTORY_MAX - 1; i > 0; i--)
                command_history[i] = command_history[i - 1];

            size_t new_command_bytes = strlen(data) + 1;
            command_history[0] = malloc(new_command_bytes);
            strncpy(command_history[0], data, new_command_bytes);
        }

        char data_processed[BUFSIZ] = {0};
        memcpy(data_processed, data, write_position);
        bool string = false;
        for (int i = 0; i < write_position; i++)
        {
            if (data_processed[i] == ' ' && !string) data_processed[i] = 0;
            if (data_processed[i] == '\"') 
            {
                string ^= true;
                data_processed[i] = 0;
            }
        }
        int bytes_left = write_position;
        char* first_arg = find_first_arg(data_processed, &bytes_left);
        if (!first_arg) goto cmd;
        if (*first_arg == 0) goto cmd;
        if (strcmp(first_arg, "exit") == 0)
        {
            disable_raw_mode_and_exit();
        }
        else if (strcmp(first_arg, "cd") == 0)
        {
            first_arg = find_next_contiguous_string(first_arg, &bytes_left);
            char* arg = find_next_contiguous_string(first_arg, &bytes_left);
            
            if (first_arg == NULL)
                continue;

            if (arg != NULL)
            {
                fprintf(stderr, "cd: too many arguments\n");
                continue;
            }
            if (chdir(first_arg) != 0)
            {
                char buf[PATH_MAX + 4];
                snprintf(buf, PATH_MAX + 4, "cd: %s", first_arg);
                perror(buf);
            }
        }
        else 
        {
        cmd:
            set_raw_mode(false);
            int status = system(data);
            set_raw_mode(true);

            if (status != -1) 
            {
                if (WIFSIGNALED(status))
                    putchar('\n');
            }

            char buf[16];
            snprintf(buf, sizeof(buf), "%d", WEXITSTATUS(status));
            setenv("?", buf, true);
            // printf("%d\n", exit_code);
        }
    }
    disable_raw_mode_and_exit();
}