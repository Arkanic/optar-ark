// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include <stdio.h>
#include <stdlib.h>

#include "lib/optark.h"
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
		"--help      -h                 display this message\n"
		"--format <format>              generate output in this paper format. supports ISO A0-6 and standard US paper sizes.\n"
//		"--landscape -l                 use landscape format for output. This preserves the text at the bottom better for smaller sizes.\n"
		"--density <density>            pixel density of the generated output. Higher density means more content stored per page,\n"
		"                               but increases the printer and scanner precision required. 3.5 is a good default for inkjet printers.\n"
		"\n"
		"Notes:\n"
		"Optar will default to A4 size with a pixel density of 3.5 unless otherwise specified.\n"
	);
}

struct {
	double density;
	struct PageDimensions *format;
	unsigned short landscape;
} configuration;

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

void formatarg_cb(char *format) {
	struct PageDimensions *dimension = dimensions_get(format);
	if(!dimension) { // if dimension not found
		fprintf(stderr, "Paper size \"%s\" was not found.\n", format);

		// TODO: print all paper sizes

		exit(1);
	}
	configuration.format = dimension;
}
struct ArgHandle formatarg = {
	.name = "format",
	.datafield = 1,
	.handlearg = &formatarg_cb
};

void densityarg_cb(char *raw) {
	sscanf(raw, "%lf", &configuration.density);
}
struct ArgHandle densityarg = {
	.name = "density",
	.datafield = 1,
	.handlearg = &densityarg_cb
};
/*
void landscapearg_cb(char *dummy) {
	configuration.landscape = 1;
}
struct ArgHandle landscapearg = {
	.name = "landscape",
	.shortname = 'l',
	.datafield = 0,
	.handlearg = &landscapearg_cb
};
*/

static struct ArgHandle *arghandles[] = {&helparg, &formatarg, &densityarg, /*&landscapearg*/};

int main(int argc, char *argv[]) {
	// set default
	configuration.density = 3.5;
	configuration.format = dimensions_get("A4");
	configuration.landscape = 0;

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
/*
	struct PageDimensions modified;
	if(configuration.landscape) {
		dimensions_landscape(&modified, configuration.format);
		configuration.format = &modified;
	}
*/
	prefill_pageformat(&format); // sane defaults
	dimensions_createconfig(&format, configuration.format, configuration.density);
	optar_file(&format, inputoutput[0], inputoutput[1]);

	return 0;
}