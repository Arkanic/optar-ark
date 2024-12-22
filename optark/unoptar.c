/* (c) GPL 2007 Karel 'Clock' Kulhavy, Twibright Labs */
// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include <stdio.h> /* printf */
#include <stdlib.h> /* malloc */
#include <math.h> /* floor */
#include <string.h> /* memcpy */
#include <assert.h> /* assert */
#include <png.h> /* libpng. Sometimes is in /usr/local/include/libpng/png.h, but that is taken care of using -I/usr/local/include/libpng in the Makefile. */

#include "lib/lib.h"
#include "lib/parity.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Crosses will be resynced with precision of FINESTEP pixels */
#define FINE_CROSS_RESYNC
#define FINESTEP 0.25
/* Define to disable repairing bit by Hamming codes */

/* Takes only unsigned integers, returns real value, if out of range returns
 * white, doens't threshold*/
#define getpixu(x, y) \
((x) >= width || (y) >= height ? 0xff : ary[(x) + (y) * width])

/* Integers in corners */
#define writepix(x, y, c) writepixu((unsigned)floor(x), (unsigned)floor(y), c)
/* If out of range, doesn't write anythinig. Integers are in pixel upper
 * left corners. */
#define writepixu(x, y, c) {if ((x) < width && (y) < height) \
	newary[(x) + (y) * width] = c;}


struct PageFormat format;
struct PageConstants constants;

struct Que {
	unsigned x;
	unsigned y;
};

static unsigned width, height; /* In pixels, not it symbols! The whole image including
			   border, white surrounding etc. */
static unsigned char *ary; /* Allocated to width*height */
static unsigned char *newary; /* Allocated to width*height */

static unsigned long histogram[256];
static unsigned char global_cutlevel;
static unsigned char fill_global_cutlevel; /* This is always set to 50% between
					      black and white to make sure the
					      border is not accidentally broken
					      through as happened when
					      global_cutlevel was used for
					      filling. */
static unsigned char average; /* Average pixel value */
static unsigned long corners[4][2]; /* UL, UR, LL, LR / x, y. Integers in pixel upper
			   left corners. */
static unsigned long leftedge, rightedge, topedge, bottomedge; /* Coordinates,
	minima/maxima of the corner coordinates. */
double ***crosses; //[constants.format->xcrosses][constants.format->ycrosses][2]; [x][y][coord]. Integers in pixel upper left corners.
float **cutlevels; //[constants.format->xcrosses][constants.format->ycrosses]; Each cross has it's own cutlevel based on how it came out printed.
static int chalf_fine; /* Larger chalf for fine search */
static int chalf; /* In the input image, measured in input image pixels!
		   Important difference - in the decoding, the crosses are
		   assumed twice as small!

		   Calculated by find_corners. */
static float *search_area; /* Allocated as soon as chalf is known. Width 4*chalf+1,
		       height 4*chalf+1. The additional "+1" is for a row
		       (topmost) and column (leftmost) of zeroes which are
		       a result of integration.

		       Before integration, position x,y (1...4*chalf) says
		       pixel value, where the middle of the cross is
		       just at the boundary between pixels [2*chalf] and
		       [2*chalf+1].
		       After integration, each pixel says integral including
		       that pixel. */

static unsigned long bad_01, bad_10; /* Flipped from 0 to 1 (black dirt),
					flipped from 1 to 0 (white dirt) */
static unsigned long bad_total;
static unsigned long irreparable;

/* These macros shift coordinates by given amount of input pixels parallel
 * with recording axes. */
#define PSHIFTX(x, dx, dy) ((x) + (dx) * pixelhx + (dy) * pixelvx)
#define PSHIFTY(y, dx, dy) ((y) + (dx) * pixelhy + (dy) * pixelvy)

static double pixelhx, pixelhy, pixelvx, pixelvy; /* Horizontal (right) and vertical
(down) pixel vectors.They are initialized as soon as the corners are determined
and their length is equal to 1.  Used to compensate out rotation. System:
+x right, +y down */
static double hpixel, vpixel; /* Pixel size calculated from the horizontal
			 and vertical corner distance */
static struct Que *que;
static struct Que *que_end; /* First invalid */
static struct Que *rptr, *wptr;
static FILE *input_stream;
static double output_gamma = 0.454545; /* What gamma the debug output has
			      (output number=number of photons ^ gamma) */
static long format_height; /* Used to be the HEIGHT macro. */
static unsigned long golay_stats[5]; /* 0, 1, 2, 3, 4 damaged bits */

/* -------------------- MAGIC CONSTANTS -------------------- */
static double unsharp_mask = 7 /* 0.7 */; 
static double unsharp_dist = 1; /* 1 means that the neighbouring pixels will be
				 sampled.  0.5 that halfway to the neighbouring
				 pixels will be sampled etc. Only values up to
				 1.0 make sense. */
static float sync_white_cut = 0.10; /* The cut used for positioning, not
				    quantization */
static float white_cut = 0.06; /* 0 means cut in the black level, 1 cut in the
			       white level, 0.5 cut in the middle etc. */
static double minmax_filter = 0.5; /* Dust/scratch removal filter. The input pixel size
			     is multiplied with this and rounded down. Then so
			     many cycles of max are performed and then the same
			     amount of cycles of min. */
static double pixel_blur = 0.25; /*0.155  Approximate width of blur blot in terms of input
			    pixel. */
static double cross_trim = 0.75; /* Such amount of input pixels (the big ones) will be
			   trimmed from the cross prior to performing the fine
			   search. */
/* -------------------- END OF MAGIC CONSTANTS -------------------- */

/* Allocates and fills in gamma table */
static unsigned char *make_gamma_table(float gamma) {
	unsigned char *t=malloc(256);
	if(!t) {
		fprintf(stderr, "Cannot allocated gamma table of 256 bytes!\n");
		exit(1);
	}

	for(int i = 0; i < 256; i++) {
		/* White level is 255 not 256! */
		float r = 255 * pow((float)i / 255, gamma);
		t[i] = floor(r + 0.5);
	}

	return t;
}

static void dump_newary(char *fname) {
	unsigned char *gamma_table = make_gamma_table(output_gamma);
	/* Translate from linear photometric back to gamma compressed. The
	 * output file will have the same gamma as the input one. */
	for(unsigned char *ptr = newary; ptr < newary + width * height; ptr++) *ptr = gamma_table[*ptr];
	free(gamma_table);

	FILE *f = fopen(fname, "w");
	if(!f) {
		fprintf(stderr, "Warning: cannot open dumpfile %s: ", fname);
		return; /* No dump */
	}

	fprintf(f,"P5\n"
		"%u %u\n"
		"255\n",
		width, height);

	fwrite(newary, width, height, f);
	fclose(f);
}

/* Gamma corrects to photon counts and calculates histogram of the gamma
 * corrected image. */
static void calc_histogram(void) {
	unsigned char *end = ary + (unsigned long)width * height;
	unsigned long long total = 0;

	memset(histogram, 0, sizeof(histogram));
	for(unsigned char *ptr = ary; ptr < end; ptr++){
		histogram[*ptr]++;
	}

	/* Calculate the sum of the histogram */
	{
		for(int i = 0; i < sizeof(histogram) / sizeof(*histogram); i++)
			total += (unsigned long long)i * histogram[i];
	}

	average = (total + ((width * height) >> 1)) / (width * height);
	fprintf(stderr, "Average pixel value %u\n", average);
}

/* Analyzes, determines the cut level */
static void analyze_cutlevel(void) {
	float white, black;
	unsigned long black_pixels, white_pixels;
	float white_rms, black_rms; /* At the end they will be RMS of the
				       distance from average */
#define MAXITER 32
	
	int lastcutlevel;

	fill_global_cutlevel = global_cutlevel = average;

	int iter; /* max. MAXITER iterations */
	/* The second guess uses global_cutlevel from the first guess */
	for(iter = 0; iter < MAXITER; iter++) {
		lastcutlevel = global_cutlevel;
		white_rms = black_rms = 0;
		black_pixels = white_pixels = 0;
		for(int i = 0; i < global_cutlevel; i++) {
			black_rms += (float)histogram[i] * (global_cutlevel - i) * (global_cutlevel - i);
			black_pixels += histogram[i];
		}
		for(int i = global_cutlevel + 1; i < sizeof(histogram) / sizeof(*histogram); i++) {
			white_rms += (float)histogram[i] * (i - global_cutlevel) * (i - global_cutlevel);
			white_pixels += histogram[i];
		}

		/* Convert square sums to mean square */
		white_rms /= white_pixels;
		black_rms /= black_pixels;

		/* Convert MS to RMS */
		white_rms = sqrt(white_rms);
		black_rms = sqrt(black_rms);

		white = global_cutlevel + white_rms;
		black = global_cutlevel - black_rms;

		/* Cutlevel determined by sync_white_cut. sync_white_cut=0 means
		 * cut at black level, 0.5 cut in the middle, 1 cut at the
		 * white level */
		global_cutlevel = floor(white * sync_white_cut + black * (1 - sync_white_cut) + 0.5);
		fill_global_cutlevel = floor(white * 0.5 + black * 0.5 + 0.5);
		fprintf(stderr,"Black %G, white %G, "
			"cutlevel %u (0x%02x), "
			"fill cutlevel %u (0x%02x)\n",
			black, white,
			global_cutlevel, global_cutlevel,
			fill_global_cutlevel, fill_global_cutlevel
		);
		if(global_cutlevel == lastcutlevel) break; /* Stable state reached */
	}
	if(iter==MAXITER) fprintf(stderr," Warning: cutting point analysis didn't converge in %u iterations.\n", MAXITER);
}

/* Returns -1 if fails. xin, yin are coordinates of the starting
 * corner. dx is which way to go away from the corner horizontally and
 * dy vertically. *outx, *outy will contain the address of the first black pixel
 * or (-1, -1) if none is found. */
static void diag_scan(int *outx, int *outy, int xin, int yin, int dx, unsigned int dy) {
	int xbegin, x, y;
	unsigned int ctr, len;

	for(xbegin = xin, len = 1; xbegin < MIN(width, height); xbegin += dx, len++) {
		x = xbegin;
		y = yin;
		for(ctr = len; ctr; ctr--, x -= dx, y += dy) {
			if(getpixu(x, y) < global_cutlevel) {
				*outx = x;
				*outy = y;
				return;
			}
		}
	}
	*outx =- 1;
	*outy =- 1;
}

/* Protection against buffer overrun. The coordinate is the coordinate of
 * the mark center in a system where integers are in pixel upper left
 * corners. Upper left pixel of the mark will be white. */
static void mark(double x, double y) {
	unsigned xu, yu;
	xu = floor(x + 0.5);
	yu = floor(y + 0.5);

	writepixu((xu),     (yu),     0xff);
	writepixu((xu - 1), (yu - 1), 0xff);
	writepixu((xu - 1), yu,       0);
	writepixu((xu),     (yu - 1), 0);
}

static void normalize_vector(double *x, double *y) {
	double vectorlen = sqrt(*x * *x + *y * *y);
	*x /= vectorlen;
	*y /= vectorlen;
}

/* Returns angle, from 0 to 360, of a normalized vector. System: +y up. */
static double angle(double x, double y) {
	double deg;
	if(abs(x) > abs(y)) {
		/* Horizontal */
		deg = 180 / M_PI * asin(y);
		if(x < 0) deg = 180 - deg;
	} else {
		/* Vertical */
		deg = 180 / M_PI * asin(x);
		if(y < 0) deg = deg - 90;
		else deg = 90 - deg;
	}
	return deg;
}

/* Puts it -180...+180 */
static double normalize_angle(double a) {
	return remainder(a, 360);
}

static void find_corners(void) {
	int x, y;
	diag_scan(&x, &y, 0, 0, 1, 1);

	if(x < 0) {
		static char failure[] = "failure_debug.pgm";
		fprintf(stderr, "Error: cannot find upper left corner\n");
fail:
		fprintf(stderr, "See failure_debug.pgm why.\n");
		memcpy(newary, ary, (unsigned long)width * height);
		dump_newary(failure);
		exit(1);
	}
	corners[0][0] = x;
	corners[0][1] = y;

	diag_scan(&x, &y, width - 1, 0, -1, 1);
	if(x < 0) {
		fprintf(stderr, "Error: cannot find upper right corner\n");
		goto fail;
	}
	corners[1][0] = x + 1;
	corners[1][1] = y;

	diag_scan(&x, &y, 0, height - 1, 1, -1);
	if(x < 0) {
		fprintf(stderr, "Error: cannot find lower left corner\n");
		goto fail;
	}
	corners[2][0] = x;
	corners[2][1] = y + 1;

	diag_scan(&x, &y, width - 1, height - 1, -1, -1);
	if(x < 0) {
		fprintf(stderr, "Error: cannot find lower right corner\n");
		goto fail;
	}
	corners[3][0] = x + 1;
	corners[3][1] = y + 1;

	leftedge = MIN(corners[0][0], corners[2][0]);
	rightedge = MAX(corners[1][0], corners[3][0]);
	topedge = MIN(corners[0][1], corners[1][1]);
	bottomedge = MAX(corners[2][1], corners[3][1]);

	hpixel = (corners[1][0] + corners[3][0] - corners[0][0] - corners[0][0]) / 2.0 / constants.width;
	vpixel = (corners[2][1] + corners[3][1] - corners[0][1] - corners[1][1]) / 2.0 / format_height;
	fprintf(stderr, "One bit is %G horizontal pixels and %G vertical pixels.\n", hpixel, vpixel);

	{
		/* Take only half to prevent spurious resync to an edge of the
		 * cross when the data mimic the other half of the cross. */
		unsigned int hchalf = hpixel * constants.format->chalf * 0.5;
		unsigned int vchalf = vpixel * constants.format->chalf * 0.5;

		chalf = MIN(hchalf, vchalf); 
		/* Round to zero to make sure we don't catch any chaff */

		/* Trim the cross by some fraction of input pixel to remove
		 * the area affected by crosstalk. */
		hchalf = hpixel * (constants.format->chalf - cross_trim);
		vchalf = vpixel * (constants.format->chalf - cross_trim);

		chalf_fine = MIN(hchalf, vchalf);
	}
	
	/* Calculate the pixel vectors */
	pixelhx=((double)corners[1][0] + (double)corners[3][0]
		   - (double)corners[0][0] - (double)corners[2][0]) / 2;
	pixelhy=((double)corners[1][1] + (double)corners[3][1]
		   - (double)corners[0][1] - (double)corners[2][1]) / 2;
	pixelvx=((double)corners[2][0] + (double)corners[3][0]
		   - (double)corners[0][0] - (double)corners[1][0]) / 2;
	pixelvy=((double)corners[2][1] + (double)corners[3][1]
		   - (double)corners[0][1] - (double)corners[1][1]) / 2;

	/* Normalize the horizontal vector (which may not be exactly
	 * horizontal */
	normalize_vector(&pixelhx, &pixelhy);
	/* Normalize the vertical vector (which may not be exactly
	 * vertical */
	normalize_vector(&pixelvx, &pixelvy);
	fprintf(stderr, "Input horizontal pixel vector %G,%G, vertical %G,%G. skew %G deg, perpendicularity %G deg.\n",
			pixelhx, pixelhy, pixelvx, pixelvy,
			normalize_angle(angle(pixelhx, -pixelhy) + angle(pixelvx, -pixelvy) + 90) / 2,
			angle(pixelhx, -pixelhy) - angle(pixelvx, -pixelvy)
	);

	{
		unsigned long bytes = (long)(4 * chalf + 1) * (4 * chalf + 1) * sizeof(*search_area);
		search_area = malloc(bytes);
		if(!search_area) {
			fprintf(stderr, "Cannot allocate search area of %lu bytes\n", bytes);
			exit(1);
		}
	}

	fprintf(stderr, "Allocating search area of %u x %u (%u) pixels.\n", chalf << 1, chalf << 1, (chalf * chalf) << 2);

	fprintf(stderr,"Upper corners at %lu, %lu and %lu, %lu,\n"
		"lower corners at %lu, %lu and %lu, %lu.\n"
		"Cross half for searching is %d x %d input pixels.\n",
		corners[0][0], corners[0][1], corners[1][0], corners[1][1],
		corners[2][0], corners[2][1], corners[3][0], corners[3][1],
		chalf, chalf);

}

static double bilinear(double ul, double ur, double ll, double lr, double hpar, double vpar) {
	double u = ur * hpar + ul * (1 - hpar); // upper
	double l = lr * hpar + ll * (1 - hpar); // lower

	return l * vpar + u * (1 - vpar);
}

static float bilinearf(float ul, float ur, float ll, float lr, float hpar, float vpar) {
	float u = ur * hpar + ul * (1 - hpar); // upper
	float l = lr * hpar + ll * (1 - hpar); // lower

	return l * vpar + u * (1 - vpar);
}

/* x,y with integers in centers of pixels */
static float get_pixel_interp(double x, double y) {
	unsigned xi, yi; /* Integer versions, rounded down */

	/* Supports even extrapolation, but should be never necessary */
	if(x < 0) xi = 0; else xi = floor(x);
	if(y < 0) yi = 0; else yi = floor(y);

	/* Make it faster, only little precision needed. */
	return bilinearf(getpixu(xi, yi),     getpixu(xi + 1, yi),
		    		 getpixu(xi, yi + 1), getpixu(xi + 1, yi + 1),
		    		 x - xi, y - yi);
}

/* Samples pixels and performs correction(s) */
static float pixel_correct_sample(double x, double y) {
	double hdist = hpixel * unsharp_dist;
	double vdist = vpixel * unsharp_dist;

	/* First sum, later average */
	float avg = get_pixel_interp(
		PSHIFTX(x, -hdist, 0),
		PSHIFTY(y, -hdist, 0));
	avg += get_pixel_interp(
		PSHIFTX(x, hdist, 0),
		PSHIFTY(y, hdist, 0));
	avg += get_pixel_interp(
		PSHIFTX(x, 0, vdist),
		PSHIFTY(y, 0, vdist));
	avg += get_pixel_interp(
		PSHIFTX(x, 0, -vdist),
		PSHIFTY(y, 0, -vdist));
	avg /= 4;


	float val = get_pixel_interp(x, y);
	val += unsharp_mask * (val - avg); /* Emphasize the distance from average */
	return val;
}

/* Returns difference from global_cutlevel. Integers in centers of pixels. Interpolates
 * for nonintegral coordinates. */
static float diffpix(double x, double y) {
	return get_pixel_interp(x, y) - global_cutlevel;
}

/* Calculates a correlation with a cross. x and y are coords of the cross
 * center with integers in UL corners of pixels. */
static float cross_correl(double x, double y) {
	/* -0.5 for conversion corners -> centers, +0.5 for conversion
	 * cross center -> sample point 1/2 pixel away from the cross
	 * center -> No addition at all */
	float sum = 0;
	for(double dx = 0; dx < chalf_fine; dx++) {
		for(double dy = 0; dy < chalf_fine; dy++) {
			sum -= diffpix(x + dx, y + dy);
			sum -= diffpix(x - 1 - dx, y - 1 - dy);
			sum += diffpix(x + dx, y - 1 - dy);
			sum += diffpix(x - 1 - dx, y + dy);
		}
	}

	return sum;
}

/* Range from 0 to 4*chalf, inclusive */
static float getsearch(int xpos, int ypos) {
	assert(xpos >= 0);
	assert(xpos <= 4 * chalf);
	assert(ypos >= 0);
	assert(ypos <= 4 * chalf);

	return search_area[ypos * (4 * chalf + 1) + xpos];
}

/* xpos says the cross offset. 0,0 means at the original position from around
 * which the search_area was loaded. The range is -chalf to chalf
 * (inclusive). The input must be in that range, otherwise crash */
static float cross_correl_search(int xpos, int ypos) {
	float sum;

	assert(xpos >= -chalf);
	assert(xpos <= chalf);
	assert(ypos >= -chalf);
	assert(ypos <= chalf);

	/* Normalize the xpos and ypos to mean the cross center in array
	 * indices. 0 means array left edge, 2*chalf array center, 4*chalf array
	 * right edge. */

	xpos += 2 * chalf;
	ypos += 2 * chalf;

	/* Center */
	sum = -4 * getsearch(xpos, ypos);

	/* Middles of sides */
	sum += 2 * (
		getsearch(xpos - chalf, ypos) +
		getsearch(xpos + chalf, ypos) +
		getsearch(xpos, ypos - chalf) +
		getsearch(xpos, ypos + chalf)
	);

	/* Corners */
	sum -= (
		getsearch(xpos - chalf, ypos - chalf) +
		getsearch(xpos - chalf, ypos + chalf) +
		getsearch(xpos + chalf, ypos - chalf) +
		getsearch(xpos + chalf, ypos + chalf)
	);

	return sum;
}

/* After this, pixel [0][0] means integral from [0][0] to [1][1] (!), etc. */
static void integrate_search_area(void) {
	/* Horizontal integration */
	float *ptr = search_area;
	for(int y = 0; y <= 4 * chalf; y++) {
		ptr++;
		for(int x = 1; x <= 4 * chalf; x++) {
			ptr[0] += ptr[-1];
			ptr++;
		}
	}

	ptr = search_area + 4 * chalf + 1;

	/* Vertical integration */
	for(; ptr < search_area + (4 * chalf + 1) * (4 * chalf + 1); ptr++) {
		ptr[0] += ptr[-4 * chalf - 1];
	}
}

static void cross_stats(unsigned int cx, unsigned int cy) {
	double centerx = crosses[cx][cy][0];
	double centery = crosses[cx][cy][1];
	int hpixelhalf = floor(hpixel * (constants.format->chalf - cross_trim));
	int vpixelhalf = floor(vpixel * (constants.format->chalf - cross_trim));

	float black_rms = 0, white_rms = 0;
	long val;
	unsigned long whitepixels = 0, blackpixels = 0;
	float cutlevel_result;

	for(int xoff = -hpixelhalf; xoff <= hpixelhalf; xoff++) {
		for(int yoff= -vpixelhalf; yoff <= vpixelhalf; yoff++) {
			val = get_pixel_interp(
				PSHIFTX(centerx, xoff, yoff),
				PSHIFTY(centery, xoff, yoff)
			);

			if(val > global_cutlevel) {
				/* White */
				white_rms += (val - global_cutlevel) * (val - global_cutlevel);
				whitepixels++;
			} else if(val < global_cutlevel) {
				/* Black */
				black_rms += (val - global_cutlevel) * (val - global_cutlevel);
				blackpixels++;
			}
		}
	}

	if(!(whitepixels && blackpixels)) {
		cutlevel_result = global_cutlevel;
		/* Impossible to determine, use default global_cutlevel */
	} else {
		white_rms = sqrt(white_rms / whitepixels);
		black_rms = sqrt(black_rms / blackpixels);
		float white = global_cutlevel + white_rms;
		float black = global_cutlevel - black_rms;
		cutlevel_result = white * (white_cut) + black * (1 - white_cut);
	}
	cutlevels[cx][cy] = cutlevel_result;

	fprintf(stderr,"%02x ", (int)floor(cutlevel_result + 0.5));
}

/* Center of search area is in a system where the integers are in the
 * corners. */
static void load_search_area(double centerx, double centery) {
	float *ptr = search_area;
	for(int yoff = -2 * chalf - 1; yoff < 2 * chalf; yoff++) {
		for(int xoff = -2 * chalf - 1; xoff < 2 * chalf; xoff++) {
			if(yoff == -2 * chalf - 1 || xoff == -2 * chalf - 1) {
				/* Zero row/column */
				*ptr = 0;
			} else {
				/* No translation necessary since centers
				 * in corners automatically translate to
				 * centers of adjacent pixels expressed
				 * in a system with integers in centers. */
				*ptr = diffpix(PSHIFTX(centerx, xoff, yoff), PSHIFTY(centery, xoff, yoff));
			}
			ptr++;
		}
	}
}

/* The coords are with integers in corners */
static void resync_cross(double *coordpair) {
	double xmax, ymax; /* Later it's calculated in which pixel position
			      the maximum was calculated, with subpixel
			      precision. Coords of cross center with integers
			      in corners. */
	float result;

	load_search_area(coordpair[0], coordpair[1]);
	integrate_search_area(); /* Precalculates - dynamic programming */

	/* Step during which the maximum was reached */
	/* Preload with a default */
	int xoffmax = 0;
	int yoffmax = 0;

	/* How much was reached during maximum */
	float max = cross_correl_search(xoffmax, yoffmax);

	/* xoff, off - Symmetric step offsets for search stepping always
	by 1 and once meaning 1, once meaning a subpixel
	step. They perform steps parallel to the recording
	axes. */
	/* Rough search - 1 pixel step */
	for(int xoff = -chalf; xoff <= chalf; xoff++) {
		for(int yoff = -chalf; yoff <= chalf; yoff++){
			result = cross_correl_search(xoff, yoff);
			if(result > max) {
				max = result;
				xoffmax = xoff;
				yoffmax = yoff;
			}
		}
	}
	xmax = PSHIFTX(coordpair[0], xoffmax, yoffmax);
	ymax = PSHIFTY(coordpair[1], xoffmax, yoffmax);

#ifdef FINE_CROSS_RESYNC
#define HALFRANGE (0.5 / FINESTEP) /* 0.5 means 0.5 of small input pixel - no
				    need to search more since then it would be
				    caught by the neighbouring coarse search
				    try. */
	/* Load the fine search initial maximum position */
	xoffmax = 0;
	yoffmax = 0;

	/* This must be here since cross_correl and cross_correl_search
	 * return the result scaled by a different factor. */
	max = cross_correl(xmax, ymax);

	/* Fine search, FINESTEP pixels/ step. Counting in 0.25 steps */
	for(int xoff = -HALFRANGE; xoff <= HALFRANGE; xoff++) {
		for(int yoff = -HALFRANGE; yoff <= HALFRANGE; yoff++){
			/* This is not using the search area anymore! */
			result = cross_correl(
				PSHIFTX(xmax, (double)xoff * FINESTEP,
							  (double)yoff * FINESTEP),
				PSHIFTY(ymax, (double)xoff * FINESTEP,
							  (double)yoff * FINESTEP));
			if(result > max) {
				max = result;
				xoffmax = xoff;
				yoffmax = yoff;
			}
		}
	}
	/* Save the fine search result */
	xmax = PSHIFTX(xmax, (double)xoffmax * FINESTEP, (double)yoffmax * FINESTEP);
	ymax = PSHIFTY(ymax, (double)xoffmax * FINESTEP, (double)yoffmax * FINESTEP);

#endif /* FINE_CROSS_RESYNC */

	/* Store the output */
	coordpair[0] = xmax;
	coordpair[1] = ymax;
}

static void sync_crosses(void) {
	/* Calculate the estimated cross pitch vectors */
	double rightx = ((double)corners[1][0] + corners[3][0] - corners[0][0] - corners[2][0]) / 2 * constants.format->cpitch / constants.width;
	double righty = ((double)corners[1][1] + corners[3][1] - corners[0][1] - corners[2][1]) / 2 * constants.format->cpitch / constants.width;
	double downx =  ((double)corners[2][0] + corners[3][0] - corners[0][0] - corners[1][0]) / 2 * constants.format->cpitch / format_height;
	double downy =  ((double)corners[2][1] + corners[3][1] - corners[0][1] - corners[1][1]) / 2 * constants.format->cpitch / format_height;

	/* Load the upper left cross with an estimate of it's position */
	crosses[0][0][0] = bilinear(
		corners[0][0], corners[1][0],
		corners[2][0], corners[3][0],
		(double)(constants.format->border + constants.format->chalf) / constants.width,
		(double)(constants.format->border + constants.format->chalf) / format_height
	);
	crosses[0][0][1] = bilinear(
		corners[0][1], corners[1][1],
		corners[2][1], corners[3][1],
		(double)(constants.format->border + constants.format->chalf) / constants.width,
		(double)(constants.format->border + constants.format->chalf) / format_height
	);

	fprintf(stderr,"Finding crosses (%u lines), numbers indicate "
			"individual cutlevels:\n", constants.format->ycrosses);

	// cross number
	for (unsigned int cy = 0; cy < constants.format->ycrosses; cy++) {
		fprintf(stderr, "%3u: ", cy);
		for(unsigned int cx = 0; cx < constants.format->xcrosses; cx++) {
			if(cx > 0) {
				/* Copy from left */
				crosses[cx][cy][0] = crosses[cx - 1][cy][0] + rightx;
				crosses[cx][cy][1] = crosses[cx - 1][cy][1] + righty;
			} else if(cy > 0) {
				/* Copy from above */
				crosses[cx][cy][0] = crosses[cx][cy - 1][0] + downx;
				crosses[cx][cy][1] = crosses[cx][cy - 1][1] + downy;
			}/* else already preloaded */
			resync_cross(crosses[cx][cy]);
			cross_stats(cx, cy);
		}

		putc('\n', stderr);
	}
}

/* x,y coords in bit matrix. 0,0 is in the upper left cross UL corner.
 * Returns pixel position with integers in centers of pixels. Interpolates
 * also the cutlevel */
static void bit_coord(double *xout, double *yout, float *cutlevel, int x, int y) {
	/* First find the cross numbers */
	/* Division of negative numbers is probably undefined in C! */
	unsigned int cx, cy; /* Cross number */
	if(x < constants.format->chalf) cx = 0;
	else cx = (x - constants.format->chalf) / constants.format->cpitch;
	if(y < constants.format->chalf) cy = 0;
	else cy = (y - constants.format->chalf) / constants.format->cpitch;
	if(cx > constants.format->xcrosses - 2) cx = constants.format->xcrosses - 2;
	if(cy > constants.format->ycrosses - 2) cy = constants.format->ycrosses - 2;

	/* Now subtrack cross coordinate */
	x -= cx * constants.format->cpitch + constants.format->chalf;
	y -= cy * constants.format->cpitch + constants.format->chalf;
	/* x,y now the remainders. Can be negative or more than constants.format->cpitch! */

	/* Calculate double precision remainders about from 0 to 1 (not always) */
	double xrem = ((double)x + 0.5) / constants.format->cpitch;
	double yrem = ((double)y + 0.5) / constants.format->cpitch;

	double xd = bilinear(
		crosses[cx][cy][0],     crosses[cx + 1][cy][0],
		crosses[cx][cy + 1][0], crosses[cx + 1][cy + 1][0],
		xrem, yrem
	);
	double yd = bilinear(
		crosses[cx][cy][1],     crosses[cx + 1][cy][1],
		crosses[cx][cy + 1][1], crosses[cx + 1][cy + 1][1],
		xrem, yrem
	);

	if(cutlevel) {
		*cutlevel = bilinearf(
			cutlevels[cx][cy],     cutlevels[cx + 1][cy],
			cutlevels[cx][cy + 1], cutlevels[cx + 1][cy + 1],
			xrem, yrem
		);
	}
	/* xd, yd are now with integers in UL corners of pixels */
	xd -= 0.5;
	yd -= 0.5;

	/* xd, yd are now with integers in centers of pixels */
	*xout = xd;
	*yout = yd;
}

static void read_payload_bit(unsigned char bit) {
	static unsigned accu = 1;

	accu <<= 1;
	accu |= bit & 1;
	if(accu & (1 << 8)){
		putchar(accu & 0xff);
		accu = 1;
	}
}

/* Cuts out given bit and shifts the upper part */
static unsigned long shrink(unsigned long in, unsigned bitpos) {
	unsigned long high;
	in &= ~(1 << bitpos); /* Zero out the bit */
	high = in;
	in &= (1UL << bitpos) - 1;
	high ^= in;
	return (high >> 1) | in;
}

static void mark_bad_bit(unsigned int x, unsigned int y, int dir) {
	if(dir) {
		/* To 1, means black dirt. Upper left edge. */
		for(unsigned int u = 0; u < leftedge; u++) writepixu(u, y, 0);
		for(unsigned int u = 0; u < topedge; u++) writepixu(x, u, 0);
	} else {
		/* To 0, means white dirt. Lower right edge. */
		for(unsigned int u = rightedge; u < width; u++) writepixu(u, y, 0);
		for(unsigned int u = bottomedge; u < height; u++) writepixu(x, u, 0);
	}

	int size = floor(2 * sqrt(hpixel * vpixel) + 0.5);
	int v = dir ? 0 : 255;

	for(int i = -size; i <= size; i++) {
		writepixu(x + i,        y - size + 1, v);
		writepixu(x + i,        y + size - 1, v);
		writepixu(x - size + 1, y + i,        v);
		writepixu(x + size - 1, y + i,        v);
	}

	if(dir == 2) v = 0x80; else v ^= 0xff;
	for(int i = -size; i <= size; i++) {
		writepixu(x + i,    y - size, v);
		writepixu(x + i,    y + size, v);
		writepixu(x - size, y + i,    v);
		writepixu(x + size, y + i,    v);
	}
}

/* Bit 0 is the one which comes at the top of the page, although it's stored in
 * bit constants.fec_largebits-1 in the Hamming register.
 *
 * 1 dir means flipped from 0 to 1 (black dirt), 0 dir means flipped
 * from 1 to 0 (white dirt ), 2 dir means unknown
 *
 * Increments bad_01 or bad_10 and bad_total only for reparable errors.
 * Leaves bad_irreparable alone. */
static void print_badbit(unsigned int symbol, unsigned int bit, unsigned int dir) {
	if(!(bad_total)) {
		fprintf(stderr,"The following coordinates have damaged bits. "
				"\",\" is black dirt, \"'\" white dirt, \":\""
				"bit which is a part of an irreparable symbol. "
				"Exclamation marks (!) indicate irreparable "
				"damage: ");
	}

	if(dir == 1) {
		bad_01++;
		bad_total++;
	} else if(dir == 0) {
		bad_10++;
		bad_total++;
	}

	int x, y;
	seq2xy(&constants, &x, &y, symbol + bit + constants.fec_syms);

	double xd, yd; // integers in centres
	bit_coord(&xd, &yd, NULL, x, y);
	xd = floor(xd + 0.5);
	yd = floor(yd + 0.5);
	mark_bad_bit(xd, yd, dir);
	assert(x >= 0);

	unsigned char delim;
	switch(dir) {
		case 0:  delim = '\''; break;
		case 1:  delim = ',';  break;
		default: delim = ':';  break;
	}

	fprintf(stderr, "%ld%c%ld ", (long)xd, delim, (long)yd);
}

static void print_badbit_finish(void) {
	if(bad_total) {
		fprintf(stderr,"\n%lu bits bad from %llu, bit error rate %G%%. %G%% black dirt, %G%% white dirt and %lu (%G%%) irreparable.\n",
			bad_total,
			constants.usedbits, 
			100 * (double)(bad_total) / constants.usedbits,
			100 * (double)bad_01 / (bad_total),
			100 * (double)bad_10 / (bad_total),
			irreparable,
			100 * (double)irreparable / (bad_total)
		);
	} else fprintf(stderr, "No bad bits!\n");


	if(constants.format->fec_order == 1) {
		fprintf(stderr,"Golay stats\n"
			       "===========\n"
			"0 bad bits      %lu\n"
			"1 bad bit       %lu\n"
			"2 bad bits      %lu\n"
			"3 bad bits      %lu\n"
			"4 bad bits      %lu\n"
			"total codewords %lu\n",
			golay_stats[0],
			golay_stats[1],
			golay_stats[2],
			golay_stats[3],
			golay_stats[4],
			golay_stats[0] + golay_stats[1] + golay_stats[2] + golay_stats[3] + golay_stats[4]
		);
	}
}

void golay_bad_bits(unsigned long right, unsigned long wrong, unsigned long symno) {
	/* 23 MSB, 0 LSB */
	for(int bit=23; bit >= 0; bit--) {
		if((right ^ wrong) & (1UL << bit)) {
			/* Error at this place */
			print_badbit(symno, 23 - bit, (wrong >> bit) & 1);
		}
	}
}

static unsigned long ungolay(unsigned long in, unsigned long symno) {
	unsigned int data = in >> 12;

	if(golay(data) == in) {
		golay_stats[0]++;
		return data; /* No error */
	}

	/* Search for a symbol that differs in max. 3 positions */
	for(data = 0; data < (1 << 12); data++) {
		unsigned int n_ones = ones(golay_codes[data] ^ in);
		if(n_ones <= 3) {
			/* Found the right answer */
			golay_bad_bits(golay_codes[data], in, symno);
			golay_stats[n_ones]++;
			return data;
		}
	}

	/* "data" is not valid anymore now! */

	/* Irreparable */
	{
		fputc('\n', stderr);
		for(int badbit = 0; badbit < 24; badbit++) print_badbit(symno, badbit, 2);
		fprintf(stderr, "!\n");
		irreparable += 4;
		bad_total += 4;
		golay_stats[4]++;
		return in >> 12; 
	}

}

/* symno is just to figure out xy when printing broken bits. Only the
 * lowest constants.fec_largebits are taken into account on input. */
static unsigned long unhamming(unsigned long in, unsigned long symno) {
	unsigned int bugpos=0;

	/* Split the shift to make sure that it works even it constants.fec_largebits
	 * is the full size of the type */
	in &= (1UL << (constants.fec_largebits - 1) << 1) - 1;
	
	if(constants.format->fec_order >= 5) {
		bugpos |= parity(in & 0xffff0000) << 4;
	}
	if(constants.format->fec_order >= 4) {
		bugpos |= parity(in & 0xff00ff00) << 3;
	}
	if(constants.format->fec_order >= 3) {
		bugpos |= parity(in & 0xf0f0f0f0) << 2;
	}

	bugpos |= parity(in & 0xcccccccc) << 1;
	bugpos |= parity(in & 0xaaaaaaaa);
	if(bugpos) {
		in ^= 1UL << bugpos;
	}
	if(parity(in)) {
		/* Bad parity */
		if(bugpos) {
			/* Irreparable */
			fprintf(stderr, "\n");
			for(unsigned int bit = 0; bit < constants.fec_largebits; bit++) print_badbit(symno, bit, 2);
			irreparable += 2;
			bad_total += 2;
			fprintf(stderr, "!\n"); /* Cannot correct */
		} else {
			/* Just flipped parity */
			print_badbit(symno, constants.fec_largebits - 1, in & 1);
		}
	} else {
		print_badbit(symno, constants.fec_largebits - 1 - bugpos, ((~in) & 1UL << bugpos));
	}

	if(constants.format->fec_order >= 5) {
		in = shrink(in, 16);
	}
	if(constants.format->fec_order >= 4) {
		in = shrink(in, 8);
	}
	if(constants.format->fec_order >= 3) {
		in = shrink(in, 4);
	}

	in >>= 3;
	return in;
}

static void read_hamming_bit(unsigned char input, unsigned long symno) {
	static unsigned int accubits;
	static unsigned long accu;

	accu <<= 1;
	accu |= input & 1;
	accubits++;
	if(accubits >= constants.fec_largebits) {
		if(constants.format->fec_order == 1) {
			accu = ungolay(accu, symno);
		} else {
			accu = unhamming(accu, symno);
		}

		for(int shift = constants.fec_smallbits - 1; shift >= 0; shift--) read_payload_bit(accu >> shift);
		accu = 0;
		accubits = 0;
	}
}

void reset_stats(void) {
	bad_01 = 0;
	bad_10 = 0;
	bad_total = 0;
	irreparable = 0;
	memset(golay_stats, 0, sizeof(golay_stats));
}

static void read_syms(void) {
	reset_stats();

	/* Hamming symbol sequence number */
	unsigned int bit;
	unsigned long seq;
	double xcoord, ycoord; /* Integers in centers */
	float pixval;
	float local_cutlevel;
	int x, y; /* 0,0 is upper left pixel of upper left cross */
	for(unsigned long hamming_sym = 0; hamming_sym < constants.fec_syms; hamming_sym++) {
		for(bit = 0; bit < constants.fec_largebits; bit++) {
			/* Bit here will correspond to bit constants.fec_smallbits-1
			 * in the Hamming register. */
			seq = hamming_sym + bit * constants.fec_syms;
			seq2xy(&constants, &x, &y, seq);
			bit_coord(&xcoord, &ycoord, &local_cutlevel, x, y);
			pixval = pixel_correct_sample(xcoord, ycoord);

			/* Possibly make a mark */
			if(!(x & 7) || !(y & 7)) {
				int writeval = floor(pixval + 0.5);
				if(writeval > 255) writeval = 255;
				else if(writeval < 0) writeval = 0;
				writeval ^= 255;

				/* Make a debug dot */
				writepix(xcoord + 0.5, ycoord + 0.5, writeval);
			}

			read_hamming_bit(pixval < local_cutlevel, hamming_sym);
		}
	}

	print_badbit_finish();
}

/* Doesn't depend on width and height. */
static void print_chan_info(void) {
	fprintf(stderr, "Unformatted channel capacity %G kB, ",                      (double)constants.width * format_height / 8 / 1000);
	fprintf(stderr, "formatted raw channel capacity %G kB, ",                    (double)constants.totalbits / 8 / 1000);
	fprintf(stderr, "net EC payload capacity %G kB, ",                           (double)constants.netbits / 8 / 1000);
	fprintf(stderr, "%llu EC symbols, ",                                         constants.fec_syms);
	fprintf(stderr, "%llu bits unused (incomplete Hamming symbol), ",            constants.totalbits-constants.usedbits);
	fprintf(stderr, "border taking %G%% of unformatted capacity, ",              100 * (1 - (double)(constants.data_width) * (constants.data_height) / constants.width / format_height));
	fprintf(stderr, "border with crosses taking %G%% of unformatted capacity, ", 100 * (1 - (double)(constants.totalbits) / constants.width / format_height));
	fprintf(stderr,"border with crosses and EC taking %G%% of "
		"unformatted capacity.\n",                                               100 * (1 - (double)(constants.netbits) / constants.width / format_height));
}

static void print_marks(void) {

	mark(corners[0][0], corners[0][1]);
	mark(corners[1][0], corners[1][1]);
	mark(corners[2][0], corners[2][1]);
	mark(corners[3][0], corners[3][1]);

	// cross number
	for(unsigned int cy = 0; cy < constants.format->ycrosses; cy++) {
		for(unsigned int cx = 0; cx < constants.format->xcrosses; cx++) {
			mark(crosses[cx][cy][0], crosses[cx][cy][1]);
			mark(PSHIFTX(crosses[cx][cy][0], chalf,  0),      PSHIFTY(crosses[cx][cy][1], chalf,  0));
			mark(PSHIFTX(crosses[cx][cy][0], -chalf, 0),      PSHIFTY(crosses[cx][cy][1], -chalf, 0));
			mark(PSHIFTX(crosses[cx][cy][0], 0,      chalf),  PSHIFTY(crosses[cx][cy][1], 0,      chalf));
			mark(PSHIFTX(crosses[cx][cy][0], 0,      -chalf), PSHIFTY(crosses[cx][cy][1], 0,      -chalf));
		}
	}
}

/* Blurs ary into newary with the following kernel:
 * 1 2 1
 * 2 4 2
 * 1 2 1
 */
static void blur_copy(void) {
	/* Round to nearest */
	int blur_cycles = floor(vpixel * hpixel * pixel_blur * pixel_blur + 0.5);

	if(blur_cycles) fprintf(stderr, "Doing %d cycles of 1 2 1 / 2 4 2 / 1 2 1 blur.\n" , blur_cycles);

	for(int cycles = 1; cycles <= blur_cycles; cycles++) {
		unsigned char *dest = newary;
		unsigned char *src = ary;
		memcpy(dest, src, width); /* Topmost row */
		dest += width;
		src += width;

		for(long yctr = height - 2; yctr > 0; yctr--) {
			*dest++ = *src++; /* Leftmost pixel */
			for(long xctr= width - 2; xctr > 0; xctr--) {
				int val = src[0] << 2;
				val +=  (src[-1] + src[1] + *(src - width) + src[width]) << 1;
				val += *(src - 1 - width) + *(src - width + 1) + src[width - 1] + src[width + 1];
				val =   (val + 8) >> 4; /* 4+2+2+2+2+1+1+1+1=16 */
				*dest++ = val;
				src++;
			}
			*dest++ = *src++; /* Rightmost pixel */
		}
		memcpy(dest, src, width); /* Bottommost row */
		memcpy(ary, newary, width * height);
		fprintf(stderr, "%d ", cycles);
	}
	if(!blur_cycles) memcpy(newary, ary, width * height);
	else fprintf(stderr, "\n");
}

/* Shifts half pixel right and down! */
static void max(void) {
	unsigned char *ptr = ary + (unsigned long)width * height;
	unsigned char *linestart;
	for(unsigned long yctr = height; yctr; yctr--) {
		linestart = ptr - width;
		ptr--;
		for(; ptr > linestart; ptr--) ptr[0] = MAX(ptr[0], ptr[-1]);
	}

	unsigned char *end = ary + width;
	for(ptr = ary + (unsigned long)width * height - 1; ptr >= end; ptr--) ptr[0] = MAX(ptr[0], *(ptr - width));
}

/* Shifts half pixel left and up! */
static void min(void){
	unsigned char *ptr, *end;
	unsigned long yctr;

	for(ptr = ary, yctr = height; yctr; yctr--) {
		for(end = ptr + width - 1; ptr < end; ptr++) {
			ptr[0] = MIN(ptr[0], ptr[1]);
		}

		ptr++;
	}

	end = ary + (unsigned long)width * (height - 1);
	for(ptr = ary; ptr < end; ptr++) ptr[0] = MIN(ptr[0], ptr[width]);
}

/* Calculate how many pixels */
static void process_minmax(void) {
	float npix = sqrt(vpixel * hpixel); /* Average pixel */
	npix *= minmax_filter;
	npix = floor(npix);

	if(npix) fprintf(stderr, "Doing %d cycles of max and %d cycles of min.\n", (int)npix, (int)npix);

	for(int i = 1; i <= npix; i++) {
		max();
		fprintf(stderr, "%d ", i);
	}
	for(int i = 1; i <= npix; i++) {
		min();
		fprintf(stderr, "%d ", i);
	}
	if(npix) fprintf(stderr,"\n");
}

/* Dimensions were originally fixed in macros. Now they are variable and can be
 * passed through commandline. This routine reclaculates them after they are
 * loaded from the commandline. */
static void init_dimensions(void) {
	format_height = 2 * constants.format->border + constants.data_height + constants.format->text_height;
}

static void que_write(unsigned int x, unsigned int y) {
	wptr->x = x;
	wptr->y = y;
	wptr++;

	if(wptr >= que_end) wptr = que;
	if(wptr == rptr) {
		fprintf(stderr, "unoptar: Floodfill que overflowed. Search "
			"for \"que=malloc\" in the program and increase the "
			"size.\n");
		exit(1);
	}
}

/* 1 OK, 0 empty */
static int que_read(unsigned int *x, unsigned int *y) {
	if(wptr == rptr) return 0; /* Empty */
	*x = rptr->x;
	*y = rptr->y;
	rptr++;
	if(rptr >= que_end) rptr = que;
	return 1;
}

static void init_que(void) {
	rptr = que;
	wptr = que;
}

static void try_copy_white(unsigned int x, unsigned int y, char test) {
	if(test && ary[(unsigned long)y * width + x] < fill_global_cutlevel) return; /* Black */
	unsigned char *destptr = newary + (unsigned long)y * width + x;
	if(!*destptr) return; /* Already copied through */
	*destptr = 0;
	que_write(x, y);
}

/* Test: test for presence of white pixel in source, otherwise test just in
 * the destination */
static void fill(unsigned int x, unsigned int y, char test) {
	init_que();
	try_copy_white(x, y, test);
	while(que_read(&x, &y)) {
		if(x + 1 < width)  try_copy_white(x + 1, y,     test);
		if(x)              try_copy_white(x - 1, y,     test);
		if(y + 1 < height) try_copy_white(x,     y + 1, test);
		if(y)              try_copy_white(x,     y - 1, test);
	}
}

/* Or newary to ary */
static void erase_dirt(void) {
	unsigned char *src, *dest;
	unsigned char *destend;
	unsigned long dirt_pixels = 0;

	for(destend = ary + (unsigned long)width * height, src = newary, dest = ary; dest < destend; src++, dest++) {
		*dest |= *src;
		dirt_pixels += *src & 1;
	}

	fprintf(stderr, "erased %lu pixels of dirt.\n", dirt_pixels);
}

/* Clobbers newary */
static void remove_dirt_from_border(void) {
	unsigned int que_size = ((unsigned long)MAX(width, height) << 1) + 5;
	/* Not that I would really know the real bound */
	que = malloc(que_size * sizeof(*que));
	if(!que) {
		fprintf(stderr, "Cannot allocate the fill que.\n");
		exit(1);
	}
	que_end = que + que_size;

	memset(newary, 0xff, (unsigned long)width * height);

	fill(0, 0, 1);
	fill(width >> 1, 0, 1);
	fill(width - 1, 0, 1);
	fill(0, height >> 1, 1);
	fill(0, height - 1, 1);
	fill(width - 1, height - 1, 1);
	fill(width - 1, height >> 1, 1);
	fill(width >> 1, height - 1, 1);
	fprintf(stderr, "white border identified, ");
	fill(width >> 1, height >> 1, 0);
	fprintf(stderr, "data area identified, ");
	/* Now white parts and the data area are filled with 0xff in newary. */
	erase_dirt();
	free(que);
}

/* Produces already linear output! */
void read_png(void) {
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr,input_stream);
	png_read_info(png_ptr, info_ptr);

	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);

	fprintf(stderr, "Input %u x %u pixels, taking %G megabytes for 2 framebuffers.\n", width, height, 2 * (float)width * height / 1e6);

	double gamma; /* gamma from the info in the file */
	if(png_get_gAMA(png_ptr, info_ptr, &gamma)) png_set_gamma(png_ptr, 1.0, gamma);
	else png_set_gamma(png_ptr, 1.0, 0.454545); /* Default gamma */

	{
		int bit_depth;
		int color_type;

		color_type = png_get_color_type(png_ptr, info_ptr);
		bit_depth = png_get_bit_depth(png_ptr, info_ptr);
		if(color_type == PNG_COLOR_TYPE_GRAY) {
			if(bit_depth < 8) {
				png_set_expand(png_ptr);
			}
			if(bit_depth == 16) {
				png_set_strip_16(png_ptr);
			}
		}
		if(color_type == PNG_COLOR_TYPE_PALETTE) {
			png_set_expand(png_ptr);
			png_set_rgb_to_gray(png_ptr, 1, -1, -1);
			/* Default weights to be used */
		}
		if(color_type & PNG_COLOR_MASK_ALPHA) {
			png_set_strip_alpha(png_ptr);
		}
		if(color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
			png_set_rgb_to_gray(png_ptr, 1, -1, -1);
			/* Default weights to be used */
		}
	}
	/* If the depth is different from 8 bits/gray, make the libpng expand
	 * it to 8 bit gray.
	 */
	int number_of_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);
	ary = malloc((unsigned long)width * height);
	newary = malloc((unsigned long)width * height);
	if(!(ary && newary)) {
		fprintf(stderr, "Cannot allocate framebuffers.\n");
		exit(1);
	}
	unsigned char **ptrs = malloc(height * sizeof(*ptrs));
	if(!ptrs) {
		fprintf(stderr, "Cannot allocate %lu bytes for auxilliary buffer\n", height * (unsigned long)(sizeof(*ptrs)));
		exit(1);
	}

	for(int y1 = 0; y1 < height; y1++) ptrs[y1] = ary + width * y1;
	for(; number_of_passes; number_of_passes--) {
		png_read_rows(png_ptr, ptrs, NULL, height);
	}

	png_read_end(png_ptr, NULL);
	free(ptrs);
	fclose(input_stream);
}


/* The file must be already opened in input_stream. filename must be long enough
 * so that .png at the end can be replaced with _debug.pgm. */
static void process_file(char *filename) {
	fprintf(stderr, "Decoding PNG file %s...\n", filename);
	read_png(); /* Reads *and* closes input_stream*/

	calc_histogram();
	analyze_cutlevel();
	/* now fill_global_cutlevel and global_cutlevel are valid */

	fprintf(stderr, "Removing dirt from the white border: ");
	remove_dirt_from_border();

	fprintf(stderr, "Searching for the corners.\n");
	find_corners(); /* Also calculates chalf and pixel vectors. */
	/* After find_corners, hpixel and vpixel are valid. */
	sync_crosses();

	/* Minmax is before blur because before blur, narrow cracks and spots
	 * can be distinguished in size from wide shallow depressions. Otherwise
	 * we couldn't distinguish them apart - we would lose information. */
	process_minmax();
	blur_copy();

	/* Prints the crashtest dummy marks. */
	print_marks();

	/* Now comes the decoding itself. */
	read_syms();
	free(ary);
	free(search_area);

	strcpy((void *)(filename + strlen(filename) - 4), "_debug.pgm");
	fprintf(stderr, "Writing debug image into %s.\n", filename);
	dump_newary(filename); /* Also recompresses with gamma. */
	free(newary);
}

static void process_files(char *base) {
	unsigned int alloclen = strlen(base) + 1 + 4 + 1 + 5 + 1 + 3 + 1;
	/* Longer filename */
	char *longer = malloc(alloclen); /* _ 0001 _ debug . pgm \0 */
	if(!longer){
		fprintf(stderr, "unoptar: cannot allocate output base\n");
		exit(1);
	}

	unsigned file_number = 0;
	while(1) {
		if(file_number >= 9999) {
			fprintf(stderr, "unoptar: Too many pages - 10,000 or more.\n");
			exit(1);
		}
		snprintf(longer, alloclen - 6,"%s_%04u.png", base, ++file_number);
		/* 6 for "_debug" */

		input_stream = fopen(longer, "r");
		if(!input_stream) {
			if (file_number == 1){
				/* We didn't have any files! */
				fprintf(stderr, "unoptar: cannot open %s: ", longer);
				free(longer);
				perror("");
				exit(1);
			}else{
				free(longer);
				return;
			}
		}
		process_file(longer); /* Clobbers longer! Automatically closes input_stream! */
	}
}

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
	
	print_pageformat(pageformat);
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
	compute_constants(&constants, &format);

	//[constants.format->xcrosses][constants.format->ycrosses][2]
	// initialize crosses (double)
	crosses = (double ***)malloc(sizeof(double **) * constants.format->xcrosses);
	if(!crosses) {
		fprintf(stderr, "Failed to allocate crosses\n");
		exit(1);
	}

	for(int x = 0; x < constants.format->xcrosses; x++) {
		crosses[x] = (double **)malloc(sizeof(double *) * constants.format->ycrosses);
		if(!crosses[x]) {
			fprintf(stderr, "Failed to allocate crosses[%d]\n", x);
			exit(1);
		}

		for(int y = 0; y < constants.format->ycrosses; y++) {
			crosses[x][y] = (double *)malloc(sizeof(double) * 2);
			if(!crosses[x][y]) {
				fprintf(stderr, "Failed to allocate crosses[%d][%d]\n", x, y);
				exit(1);
			}
		}
	}

	//[constants.format->xcrosses][constants.format->ycrosses]
	// initialize cutlevels (float)
	cutlevels = (float **)malloc(sizeof(float *) * constants.format->xcrosses);
	if(!cutlevels) {
		fprintf(stderr, "Failed to allocate cutlevels\n");
		exit(1);
	}

	for(int x = 0; x < constants.format->xcrosses; x++) {
		cutlevels[x] = (float *)malloc(sizeof(float) * constants.format->ycrosses);
		if(!cutlevels[x]) {
			fprintf(stderr, "Failed to allocate cutlevels[%d]\n", x);
			exit(1);
		}
	}

	/* This must after all dimension-related parameters are decoded. */
	init_dimensions();

	print_chan_info();
	process_files(argv[2]);

	// free cutlevels
	for(int x = 0; x < constants.format->xcrosses; x++) {
		free(cutlevels[x]);
	}
	free(cutlevels);

	// free crosses
	for(int x = 0; x < constants.format->xcrosses; x++) {
		for(int y = 0; y < constants.format->ycrosses; y++) {
			free(crosses[x][y]);
		}
		free(crosses[x]);
	}
	free(crosses);

	return 0;
}