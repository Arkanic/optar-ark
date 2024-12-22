/* (c) GPL 2007 Karel 'Clock' Kulhavy, Twibright Labs */
// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include "optark.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define TEXT_WIDTH 13 /* Width of a single letter */
#define TEXT_HEIGHT 24 /* Height of a single letter */

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

/* Functions from common.c */
extern void compute_constants(struct PageConstants *out, struct PageFormat *format);
extern void print_pageformat(struct PageFormat *format);
extern void print_pageconstants(struct PageConstants *constants);
extern void prefill_pageformat(struct PageFormat *out);
extern unsigned long parity(unsigned long in);
extern int is_cross(struct PageConstants *constants, unsigned int x, unsigned int y);
extern void seq2xy(struct PageConstants *constants, int *x, int *y, unsigned seq);

/* Counts number of '1' bits */
unsigned ones(unsigned long in);

/* Golay codes */
unsigned long golay(unsigned long in);
extern unsigned long golay_codes[4096];
