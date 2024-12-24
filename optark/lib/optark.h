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

/* Computed constants generated from format of optar page */
struct PageConstants {
	struct PageFormat *format;

	// generated values from format

	unsigned int data_width; // the rectangle bounds of data/crosses in pixels
	unsigned int data_height;
	unsigned long width; // width in pixels with border
	unsigned long height;

	// definitions for seq2xy

	// Properties of the narrow horizontal strip, with crosses
	unsigned int narrowheight;
	unsigned int gapwidth;
	unsigned long narrowwidth; // Useful width
	unsigned long long narrowpixels; // Useful pixels

	// Properties of the wide horizontal strip, without crosses
	unsigned int wideheight;
	unsigned long widewidth;
	unsigned long long widepixels;

	// Amount of raw payload pixels in one narrow-wide strip pair
	unsigned int repheight;
	unsigned long long reppixels;

	// Total bits before hamming including the unused
	unsigned long long totalbits;

	// changes based on golay/hamming via fec_order
	unsigned int fec_largebits;
	unsigned int fec_smallbits;

	// Hamming net channel capacity
	unsigned long long fec_syms;
	unsigned long long netbits; // Net payload bits
	unsigned long long usedbits; // Used raw bits to store hamming/golay symbols
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



// Page dimensions in mm
struct PageDimensions {
    char *name;
    unsigned int width;
    unsigned int height;
};

#define pd(n, w, h) {.name = #n, .width = w, .height = h}

// list of generic sizes that users may want to generate barcodes for
static struct PageDimensions dimensions[] = {
    // ISO paper sizes
    pd(a0, 841, 1188),
    pd(a1, 594, 841),
    pd(a2, 420, 594),
    pd(a3, 297, 420),
    pd(a4, 210, 297),
    pd(a5, 148, 210),
    pd(a6, 105, 148),
    pd(a7, 74, 105),
    pd(a8, 52, 74),

    // US paper sizes
    pd(letter, 216, 279),
    pd(legal, 216, 356),
    pd(tabloid, 279, 432),

    // misc
    pd(sticker, 100, 100),
    pd(metre, 1000, 1000)
};


// dimensions.c

struct PageDimensions *dimensions_get(char *name);
void dimensions_landscape(struct PageDimensions *out, struct PageDimensions *in);
void dimensions_createconfig(struct PageFormat *format, struct PageDimensions *dimensions, double density);
unsigned long long dimensions_capacity(struct PageFormat *format);