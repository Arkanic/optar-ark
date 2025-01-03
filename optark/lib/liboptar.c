/* (c) GPL 2007 Karel 'Clock' Kulhavy, Twibright Labs */
// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define width font_width
#define height font_height
#include "font.h"
#undef width
#undef height

#include "lib.h"
#include "parity.h"

struct PageConstants optarconstants;

static unsigned char *ary; //[WIDTH * HEIGHT];
static unsigned char *file_label = (unsigned char *)""; /* The filename written in the file_label */
static char *output_filename; /* The output filename */
static unsigned output_filename_buffer_size;
static unsigned char *base = (unsigned char *)"optar_out"; /* Output filename base */
static unsigned file_number;
FILE *output_stream;
FILE *input_stream;
unsigned n_pages; /* Number of pages calculated from the file length */

void dump_ary(void) {
	fprintf(output_stream,
		"P5\n%lu %lu\n255\n",
		optarconstants.width, optarconstants.height
	);

	fwrite(ary, optarconstants.width * optarconstants.height, 1, output_stream);
}

/* Only the LSB is significant. Writes hamming-encoded bits. The sequence number
 * must not be out of range! */
void write_channelbit(unsigned char bit, unsigned long seq) {
	int x, y; /* Positions of the pixel */

	bit &= 1;
	bit =- bit;
	bit =~ bit; /* White=bit 0, black=bit 1 */
	seq2xy(&optarconstants, &x, &y, seq); /* Returns without borders! */
	x += optarconstants.format->border;
	y += optarconstants.format->border;
	ary[x + y * optarconstants.width] = bit;
	seq++;
}

/* Groups into two groups of bits, 0...bit-1 and bit..., and then makes
 * a gap with zero between them by shifting the higer bits up. */
unsigned long split(unsigned long in, unsigned bit) {
	unsigned long high = in;
	in &= (1UL << bit) - 1;
	high ^= in;
	return (high << 1) | in;
}

/* Thie bits are always stored in the LSB side of the register. Only the
 * lowest FEC_SMALLBITS are taken into account on input. */
unsigned long hamming(unsigned long in) {
	in &= (1UL << optarconstants.fec_smallbits) - 1;

	in <<= 3; /* Split 0,1,2 */
	if(optarconstants.format->fec_order >= 3) {
		in = split(in, 4);
		if(optarconstants.format->fec_order >= 4) {
			in = split(in, 8);
			if(optarconstants.format->fec_order >= 5) {
				in = split(in, 16);
				in |= parity(in & 0xffff0000) << 16;
			}
			in |= parity(in & 0xff00ff00) << 8;
		}
		in |= parity(in & 0xf0f0f0f0) << 4;
	}
	in |= parity(in & 0xcccccccc) << 2;
	in |= parity(in & 0xaaaaaaaa) << 1;
	in |= parity(in);

	return in;
}

void border(void) {
	char *ptr = (char *)(void *)ary;

	memset(ptr, 0, optarconstants.format->border * optarconstants.width);
	ptr += optarconstants.format->border * optarconstants.width;
	for(unsigned int c = optarconstants.data_height; c; c--) {
		memset(ptr, 0, optarconstants.format->border);
		ptr += optarconstants.width;
		memset(ptr - optarconstants.format->border, 0, optarconstants.format->border);
	}
	memset(ptr, 0, optarconstants.format->text_height * optarconstants.width);
	ptr += optarconstants.format->text_height * optarconstants.width;
	/* BORDER bytes into the bottom border */
	memset(ptr, 0, optarconstants.format->border * optarconstants.width);
}

void cross(int x, int y) {
	unsigned char *ptr = ary + y * optarconstants.width + x;

	for (unsigned int c = optarconstants.format->chalf; c; c--, ptr += optarconstants.width){
		memset(ptr, 0, optarconstants.format->chalf);
		memset(ptr + optarconstants.format->chalf, 0xff, optarconstants.format->chalf);
		memset(ptr + optarconstants.format->chalf * optarconstants.width, 0xff, optarconstants.format->chalf);
		memset(ptr + optarconstants.format->chalf * (optarconstants.width + 1), 0, optarconstants.format->chalf);
	}
}

void crosses(void) {
	for (unsigned int y = optarconstants.format->border; y <= optarconstants.height - optarconstants.format->text_height - optarconstants.format->border - 2 * optarconstants.format->chalf; y += optarconstants.format->cpitch) {
		for (unsigned int x = optarconstants.format->border; x <= optarconstants.width - optarconstants.format->border - 2 * optarconstants.format->chalf; x += optarconstants.format->cpitch) {
			cross(x, y);
		}
	}
}

/* x is in the range 0 to DATA_WIDTH-1 */
void text_block(int destx, int srcx, int width) {
	if(destx + width > optarconstants.data_width) return; /* Letter doesn't fit */

	unsigned char *srcptr = (unsigned char *)(void *)header_data + srcx;
	unsigned char *destptr = ary + optarconstants.width * (optarconstants.format->border + optarconstants.data_height) + optarconstants.format->border + destx;

	for(int y = 0; y < optarconstants.format->text_height; y++, srcptr += font_width, destptr += optarconstants.width) {
		for(int x = 0; x < width; x++) {
			destptr[x] = header_data_cmap[srcptr[x]][0] & 0x80 ? 0xff : 0;
		}
	}
}

void label(void) {
	size_t txtsize = sizeof(char) * (optarconstants.data_width / TEXT_WIDTH);
	char *txt = (char *)malloc(txtsize);
	if(!txt) {
		fprintf(stderr, "Cannot allocate txt");
		exit(1);
	}

	snprintf(txt, txtsize, "  0-%u-%u-%u-%u-%u-%u-%u %u/%u %s",
		optarconstants.format->xcrosses, optarconstants.format->ycrosses, optarconstants.format->cpitch, optarconstants.format->chalf,
		optarconstants.format->fec_order, optarconstants.format->border, optarconstants.format->text_height,
		file_number, n_pages,
		(char *)(void *)file_label);
	unsigned int txtlen = strlen((char *)(void *)txt);

	assert(font_height == optarconstants.format->text_height);

	int source_length = TEXT_WIDTH * (127 - ' ');
	unsigned int x = font_width - source_length;
	text_block(0, source_length, x);

	for (unsigned char *ptr = (unsigned char *)(void *)txt; ptr < (unsigned char *)(void *)txt + txtlen; ptr++) {
		if(*ptr >= ' ' && *ptr <= 127) {
			text_block(x, TEXT_WIDTH * (*ptr - ' '), TEXT_WIDTH);
			x += TEXT_WIDTH;
		}
	}
}

void format_ary(void) {
	memset(ary, 0xff, optarconstants.width * optarconstants.height); /* White */
	border();
	crosses();
	label();
}

/* Always formats ary. Dumps it if it's not the first one. */
void new_file(void) {
	if(file_number) {
		dump_ary();
		fclose(output_stream);
	}

	if(file_number >= 9999) {
		fprintf(stderr, "optar: too many pages - 10,000 or more\n");
		exit(1);
	}

	snprintf(output_filename, output_filename_buffer_size, "%s_%04u.pgm", (char *)(void *)base, ++file_number);
	output_stream = fopen(output_filename, "w");
	if(!output_stream) {
		fprintf(stderr, "optar: cannot open %s for writing.\n", output_filename);
		exit(1);
	}
	format_ary();
}

void end_files(void) {
    fclose(input_stream);
}

/* That's the net channel capacity */
void write_payloadbit(unsigned char bit) {
	static unsigned long accu = 1;
	static unsigned long hamming_symbol;

	accu <<= 1;
	accu |= bit & 1;
	if(accu & (1UL << optarconstants.fec_smallbits)) {
		/* Full payload */
		int shift;

		/* Expands from FEC_SMALLBITS bits to FEC_LARGEBITS */
		if(optarconstants.format->fec_order == 1) {
			accu = golay(accu);
		} else {
			accu = hamming(accu);
		}

		if(hamming_symbol >= optarconstants.fec_syms) {
			/* We couldn't write into the page, we need to make
			 * another one */
			new_file();
			hamming_symbol = 0;
		}

		/* Write the symbol into the page */
		for(shift = optarconstants.fec_largebits - 1; shift >= 0; shift--) {
			write_channelbit(accu>>shift, hamming_symbol + (optarconstants.fec_largebits - 1 - shift) * optarconstants.fec_syms);
		}

		accu = 1;
		hamming_symbol++;
	}
}

void write_byte(unsigned char c) {
	for(int bit = 7; bit >= 0; bit--) write_payloadbit(c >> bit);
}

/* Prints the text at the bottom */
/* Makes one output file. */
void feed_data(void) {
	int c;
	while((c=fgetc(input_stream))!=EOF) {
		write_byte(c);
	}

	/* Flush the FEC with zeroes */
	for(c = optarconstants.fec_smallbits - 1; c; c--) {
		write_payloadbit(0);
	}

	dump_ary();
	fclose(output_stream);
}

void open_input_file(char *fname) {
	input_stream = fopen(fname, "r");
	if(!input_stream) {
		fprintf(stderr, "optar: cannot open input file %s: ", fname);
		perror("");
		exit(1);
	}

	if(fseek(input_stream, 0, SEEK_END)) {
		fprintf(stderr, "optar: cannot seek to the end of %s: ", fname);
		perror("");
		exit(1);
	}

	n_pages = (((unsigned long)ftell(input_stream) << 3) + optarconstants.netbits - 1) / optarconstants.netbits;
	if(fseek(input_stream, 0, SEEK_SET)) {
		fprintf(stderr, "optar: cannot seek to the beginning of %s: ", fname);
		perror("");
		exit(1);
	}
}

// EXTERNAL FUNCTIONS START HERE

int optar_file(struct PageFormat *format, char *input_filename, char *output_basename) {
    compute_constants(&optarconstants, format);

    ary = (unsigned char *)malloc(sizeof(unsigned char) * optarconstants.width * optarconstants.height);
	if(!ary) {
		fprintf(stderr, "Canont allocate full array\n");
		exit(1);
	}

    open_input_file(input_filename);

    file_label = base = output_basename;
    output_filename_buffer_size = strlen(base) + 1 + 4 + 1 + 3 + 1;
    output_filename = malloc(sizeof(char) * output_filename_buffer_size);
    if(!output_filename) {
        fprintf(stderr, "Cannot allocate output_filename\n");
        exit(1);
    }

    new_file();
    feed_data();
    end_files();

    free(output_filename);
    free(ary);

    return file_number;
}