/* (c) GPL 2007 Karel 'Clock' Kulhavy, Twibright Labs */
// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include "optark.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define TEXT_WIDTH 13 /* Width of a single letter */
#define TEXT_HEIGHT 24 /* Height of a single letter */

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
