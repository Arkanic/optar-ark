// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include <stdio.h>
#include <stdlib.h>

#include "lib/lib.h"
#include "arg.h"

struct PageFormat format;
struct PageConstants constants;

void showhelp(void) {
	fprintf(stderr,
		"\n"
		"Usage: optar <input file> [filename base]\n"
		"\n"
		"Will take the input file as data payload and produce optar_out_????.pgm"
		" which contain the input file encoded onto paper, with error"
		" correction codes, and automatically split into multiple"
		" files when necessary. Those pgm's are supposed to be printed"
		" on laser printer at least 600 DPI for example using GIMP, or use the included pgm2ps to"
		" convert them to PostScript and print for example"
		" using a PostScript viewer program.\n"
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

	prefill_pageformat(&format); // sane defaults
	optar_file(&format, inputoutput[0], inputoutput[1]);

	return 0;
}