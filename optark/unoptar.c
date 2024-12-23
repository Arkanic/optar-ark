// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include <stdio.h>
#include <stdlib.h>

#include "lib/optark.h"
#include "arg.h"

struct PageFormat format;

void showhelp(void) {
	fprintf(stderr,
		"Usage: unoptar <format> <input filename base (.png required)>\n"
		"\n"
		"Example: scan the pages as PNG at 600dpi or better, named like the following:\n"
		" example_0001.png\n example_0002.png\n ...\n example_9999.png\n"
		"The second argument is the filename base before the underscore, in this case \"example\":\n"
		"unoptar 0-33-47-24-3-1-2-24 example > example.txt\n"
		"where example.txt is replaced with whatever filename/format the original document contained.\n"
		"\n"
		"Options:\n"
		"--help -h                 display this message\n"
	);
}

void helparg_cb(char *dummy) {
	showhelp();
	exit(1);
}
struct ArgHandle helparg = {
	.name = "help",
	.shortname = 'h',
	.datafield = 0,
	.handlearg = &helparg_cb
};

static struct ArgHandle *arghandles[] = {&helparg};

static void parse_format(struct PageFormat *pageformat, char *format) {
	unsigned int dummy;

	sscanf(format, "%u-%u-%u-%u-%u-%u-%u-%u",
			&dummy,
			&pageformat->xcrosses,
			&pageformat->ycrosses,
			&pageformat->cpitch,
			&pageformat->chalf,
			&pageformat->fec_order,
			&pageformat->border,
			&pageformat->text_height);
}

/* argv:
 * input filename base (mandatory). For example "base" will produce
 * 	base_0001.png and base_0001_debug.pgm. 
 * text height (optional, defaults to 24)
 */
int main(int argc, char *argv[]) {
	char *inputoutput[2];
	int result = arg_parse(sizeof(arghandles) / sizeof(arghandles[0]), arghandles, 2, inputoutput, argc, argv);
	if(result == -1) {
		showhelp();
		exit(1);
	} else if(result == -2) {
		printf("Missing input or output filename! see usage\n");
		showhelp();
		exit(1);
	} else if(result == -3) {
		printf("Too many arguments provided\n");
		showhelp();
		exit(1);
	} else if(result > -200 && result <= -100) {
		printf("Argument \"%s\" is not a valid option\n", argv[-(result + 100)]);
		showhelp();
		exit(1);
	} else if(result > -300 && result <= -200) {
		printf("Argument \"%s\" is missing a data parameter\n", argv[-(result + 200)]);
		showhelp();
		exit(1);
	}

	parse_format(&format, inputoutput[0]);
	unoptar_file(&format, inputoutput[1]);

	return 0;
}