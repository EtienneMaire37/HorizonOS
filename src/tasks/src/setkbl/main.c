#include <horizonos.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char* kb_layouts[] = 
{
	"us_qwerty", "fr_azerty"
};

#define LAYOUTS (sizeof(kb_layouts) / sizeof(kb_layouts[0]))

void print_usage()
{
	printf("Usage: setkbl [LAYOUT]\n");
	printf("\t[LAYOUT] can be one of:\n");
	for (int i = 0; i < LAYOUTS; i++)
		printf("\t\t%s\n", kb_layouts[i]);
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		print_usage();
		return 1;
	}

	char* layout_choice_str = argv[1];
	int kb_layout_choice = -1;
	for (int i = 0; i < LAYOUTS; i++)
	{
		if (strcmp(kb_layouts[i], layout_choice_str) == 0)
		{
			kb_layout_choice = i;
			break;
		}
	}

	if (kb_layout_choice == -1)
	{
		printf("setkbl: Invalid keyboard layout `%s`\n", layout_choice_str);
		return 2;
	}

	if (set_kb_layout(kb_layout_choice + 1) == 0)
	    printf("Successfully set keyboard layout to : %s\n", kb_layouts[kb_layout_choice]);
	else
	{
		printf("Error : Couldn't modify the keyboard layout\n");
		return 3;
	}
}
