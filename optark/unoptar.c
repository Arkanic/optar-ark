// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include <stdio.h>
#include <stdlib.h>

#include "lib/optark.h"

struct PageFormat format;

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
	if(argc < 3) {
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
			"\n"
			"unoptar 0-65-93-24-3-1-2-24 scan > out.ogg\n"
			"\n"
			"out.ogg is just an example of payload file you can have any"
			" kind of file instead of that.\n"
			"\n"
		);
		exit(1);
	}

	parse_format(&format, argv[1]);

	unoptar_file(&format, argv[2]);

	return 0;
}