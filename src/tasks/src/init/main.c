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

#include <horizonos.h>

const char* kb_layouts[] = {"us_qwerty", "fr_azerty"};

int main(int argc, char** argv)
{
    tcsetpgrp(STDIN_FILENO, (pid_t)0x7fffffff);

    printf("--- Start of HorizonOS configuration ---\n\n");

    printf("Please enter your preferred keyboard layout:\n");
    for (uint8_t i = 0; i < sizeof(kb_layouts) / sizeof(char*); i++)
        printf("%u: %s    ", i + 1, kb_layouts[i]);
    putchar('\n');
    uint8_t kb_layout_choice = 0;
    while (!(kb_layout_choice >= 1 && kb_layout_choice <= 2))
    {
        printf("->");
        fflush(stdout);
        char kb_layout_choice_str[2] = { 0 };
        int ret = read(STDIN_FILENO, &kb_layout_choice_str[0], 2);
        kb_layout_choice_str[1] = 0;
        kb_layout_choice = atoi(kb_layout_choice_str);
    }

    if (set_kb_layout(kb_layout_choice) == 0)
        printf("Successfully set keyboard layout to : %s\n", kb_layouts[kb_layout_choice - 1]);
    else
        printf("Error : Defaulting to the us_qwerty keyboard layout\n");

    putchar('\n');

    execvp("term", (char*[]){"term", (char*)NULL});

    perror("init: Couldn't run `term`");
}