// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

/* configuration struct of optar page */
struct PageFormat {
	// provided values
	// xcrosses: A4=65, US Letter=67 | ycrosses: A4=93, US Letter=87
	int xcrosses; // number of horizontal crosses
	int ycrosses; // number of vertical crosses

	int cpitch; // Distance between cross centres
	int chalf; // Size of the cross half. Size of the cross is chalf*2 x chalf*2

	int fec_order; // hamming codes with pairty
	/* Can be 2 to 5 inclusive
		5 is 26/32
		4 is 11/16
		3 is 4/8
		2 is 4/1
		1 is golay codes
	*/

	int border; // thickness of border in pixels
	int text_height; // height of page footer
};


// common.c

/* Prefill PageFormat struct with sane defaults */
void prefill_pageformat(struct PageFormat *format);


// liboptar.c

/* Create a series of optar files from an input file and configuration object. Returns the number of pages generated. */
int optar_file(struct PageFormat *format, char *input_filename, char *output_basename);


// libunoptar.c

/* Parse a series of optar files from an input basename and configuration object */
void unoptar_file(struct PageFormat *format, char *input_basename);