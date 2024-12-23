// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include <stdio.h>
#include <stdlib.h>

#include "lib/lib.h"
#include "arg.h"

struct PageFormat format;
struct PageConstants constants;

void showhelp(void) {
	fprintf(stderr,
		"Usage: optar <input file> <output filename base>\n"
		"\n"
		"Takes the input file as the data payload and produces <output filename>_<0...n>.pgm files which contain"
		"the input file encoded as a multiple images with error correction, ready to be printed.\n"
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