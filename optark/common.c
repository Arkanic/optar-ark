/* (c) GPL 2007 Karel 'Clock' Kulhavy, Twibright Labs */
// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include <stdio.h> /* fprintf */

#include "optar.h"

/* Compute constants from page configuration */
void compute_constants(struct PageConstants *out, struct PageFormat *format) {
	out->format = format;

	out->data_width = format->cpitch * (format->xcrosses - 1) + 2 * format->chalf;
	out->data_height = format->cpitch * (format->ycrosses - 1) + 2 * format->chalf;
	out->width = 2 * format->border + out->data_width;
	out->height = 2 * format->border + out->data_height + format->text_height;

	out->narrowheight = 2 * format->chalf;
	out->gapwidth = format->cpitch - 2 * format->chalf;
	out->narrowwidth = out->gapwidth * (format->xcrosses - 1);
	out->narrowpixels = out->narrowheight * out->narrowwidth;

	out->wideheight = out->gapwidth;
	out->widewidth = out->width - 2 * format->border;
	out->widepixels = out->wideheight * out->widewidth;

	out->repheight = out->narrowheight + out->wideheight;
	out->reppixels = out->widepixels + out->narrowpixels;

	out->totalbits = (long)out->reppixels * (out->format->ycrosses - 1) + out->narrowpixels;

	if(format->fec_order == 1) { // golay
		out->fec_largebits = 24;
		out->fec_smallbits = 12;
	} else {                     // hamming
		out->fec_largebits = 1 << format->fec_order;
		out->fec_smallbits = out->fec_largebits - 1 - format->fec_order;
	}

	out->fec_syms = out->totalbits / out->fec_largebits;
	out->netbits = out->fec_syms * out->fec_smallbits;
	out->usedbits = out->fec_syms * out->fec_largebits;
}

/* debug print constants */
void print_pageconstants(struct PageConstants *constants) {
	struct PageFormat *format = constants->format;
	printf(
		"format:\n- xcrosses: %u\n- ycrosses: %u\n- cpitch: %u\n- chalf: %u\n"
		"- fec_order: %u\n- border: %u\n- text_height: %u\n",
		format->xcrosses, format->ycrosses, format->cpitch, format->chalf,
		format->fec_order, format->border, format->text_height
	);

	printf(
		"data_width: %lu\ndata_height: %lu\nwidth: %lu\nheight: %lu\n"
		"narrowheight: %u\ngapwidth: %u\nnarrowwidth: %lu\nnarrowpixels: %llu\n"
		"wideheight: %u\nwidewidth: %lu\nwidepixels: %llu\n"
		"repheight: %u\nreppixels: %llu\n"
		"totalbits: %llu\n"
		"fec_largebits: %u\nfec_smallbits: %u\n"
		"fec_syms: %llu\nnetbits: %llu\nusedbits: %llu\n",
		constants->data_width, constants->data_height, constants->width, constants->height,
		constants->narrowheight, constants->gapwidth, constants->narrowwidth, constants->narrowpixels,
		constants->wideheight, constants->widewidth, constants->widepixels,
		constants->repheight, constants->reppixels,
		constants->totalbits,
		constants->fec_largebits, constants->fec_smallbits,
		constants->fec_syms, constants->netbits, constants->usedbits
	);
}

/* Prefill with sane default values */
void prefill_pageformat(struct PageFormat *format) {
	format->border = 2; // 2px
	format->chalf = 3; // cross=6px x 6px
	format->cpitch = 24; // 24 pixel/cross

	format->xcrosses = 33; // A4 100kb/p
	format->ycrosses = 47; // A4 100kb/p

	format->fec_order = 1; // golay
	
	format->text_height = TEXT_HEIGHT; // constant, in px
}

/* Coordinates don't count with the border - 0,0 is upper left corner of the
 * first cross! */
int is_cross(struct PageConstants *constants, unsigned int x, unsigned int y) {
	x %= constants->format->cpitch;
	y %= constants->format->cpitch;
	return (x < 2 * constants->format->chalf) && (y < 2 * constants->format->chalf);
}

/* Returns the coords relative to the upperloeftmost cross upper left corner
 * pixel! If you have borders, you have to add them! */
void seq2xy(struct PageConstants *constants, int *x, int *y, unsigned seq) {
	unsigned int rep; /* Repetition - number of narrow strip - wide strip pair,
			 starting with 0 */

	if(seq >= constants->totalbits) {
		/* Out of range */
		*x= -1;
		*y= -1;
		return;
	}

	/* We are sure we are in range. Document structure:
	 * - narrow strip (between top row of crosses), height is
	 *   2*CHALF
	 * - wide strip, height is CPITCH-2*CHALF
	 * - the above repeats (YCROSSES-1)-times
	 * - narrow strip 
	 */
	rep = seq / constants->reppixels;
	seq = seq % constants->reppixels;

	*y = constants->repheight * rep;
	/* Now seq is sequence in the repetition pair */
	if(seq >= constants->narrowpixels) {
		/* Second, wide strip of the pair */
		*y += constants->narrowheight;
		seq -= constants->narrowpixels;
		/* Now seq is sequence in the wide strip */
		*y += seq / constants->widewidth;
		*x = seq % constants->widewidth;
	} else {
		/* First, narrow strip of the pair */
		unsigned int gap; /* Horizontal gap number */
		*x = 2 * constants->format->chalf;
		*y += seq / constants->narrowwidth;
		seq %= constants->narrowwidth;
		/* seq is now sequence in the horiz. line */
		gap = seq / constants->gapwidth;
		*x += gap * constants->format->cpitch;
		seq %= constants->gapwidth;
		/* seq is now sequence in the gap */
		*x += seq;
	}
}

/* Golay codes */
unsigned long golay(unsigned long in) {
	return golay_codes[in&4095];
}