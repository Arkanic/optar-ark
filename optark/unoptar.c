// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include <stdio.h>
#include <stdlib.h>

#include "lib/optark.h"
#include "arg.h"

struct PageFormat format;

void showhelp(void) {
	fprintf(stderr,
		"\n"
		"usage: unoptar <format> <input filename base>\n"
		"\n"
		"Example: scan the pages into PNG (not JPEG!) on 600dpi (or 1200dpi, slightly better)\n"
		"\n"
		" scan_0001.png\n"
		" scan_0002.png\n"
		" scan_0003.png\n"
		" ...\n"
		"\n"
		"Read the number sequence (format specification)"
		" from any of the papers and feed it as 1st argument"
		" to the optar, 2nd argument is the filename part"
		" before the underscore:\n"
		"unoptar 0-65-93-24-3-1-2-24 scan > out.ogg\n"
		"out.ogg is just an example of payload file you can have any"
		" kind of file instead.\n"
		"\n"
		"Options:\n"
		"--help -h      display this message\n"
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