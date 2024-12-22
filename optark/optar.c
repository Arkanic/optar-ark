// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include <stdio.h>
#include <stdlib.h>

#include "lib/lib.h"

struct PageFormat format;
struct PageConstants constants;

int main(int argc, char *argv[]) {
	if(argc < 3) {
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
		);
		exit(1);
	}

	prefill_pageformat(&format); // sane defaults
	optar_file(&format, argv[1], argv[2]);

	return 0;
}