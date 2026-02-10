#include <horizonos.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

const char* kb_layouts[] = 
{
	"us_qwerty", "fr_azerty"
};

int main()
{
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
}
