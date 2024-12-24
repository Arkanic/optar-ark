// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lib.h"

/* Get dimensions from name, NULL if not found */
struct PageDimensions *dimensions_get(char *name) {
    char *lowername = malloc(sizeof(char) * (strlen(name) + 1));
    lowername[strlen(name)] = '\0';
    if(!lowername) return NULL;
    for(int i = 0; i < strlen(name); i++) lowername[i] = tolower(name[i]);

    for(int i = 0; i < (sizeof(dimensions) / sizeof(dimensions[0])); i++) {
        if(!strcmp(dimensions[i].name, lowername)) {
            free(lowername);
            return &dimensions[i];
        }
    }

    free(lowername);
    return NULL; 
}

/* swap dimensions to landscape mode */
void dimensions_landscape(struct PageDimensions *out, struct PageDimensions *in) {
    out->name = in->name;
    out->width = in->height;
    out->height = in->width;
}

/* Get x/y crosses from provided dimensions & density (px/mm)
 * Requires other format fields to be filled in order to calculate width/height
 * Result will not be 100% accurate as page needs to fit into nearest cross size (+/- 2chalf + cpitch)
 */
void dimensions_createconfig(struct PageFormat *format, struct PageDimensions *dimensions, double density) {
    unsigned long pixel_width = dimensions->width * density;
    unsigned long pixel_height = dimensions->height * density;

    double xcrosses = ((pixel_width - 2*format->border - 2*format->chalf) / (float)format->cpitch) + 1.0f;
    double ycrosses = ((pixel_height - 2*format->border - 2*format->chalf - format->text_height) / (float)format->cpitch) + 1.0f;

    xcrosses = xcrosses + 0.5 - (xcrosses < 0);
    ycrosses = ycrosses + 0.5 - (ycrosses < 0);
    format->xcrosses = (unsigned int)xcrosses;
    format->ycrosses = (unsigned int)ycrosses;
}

/*
 * Get payload capacity of dimension/density in bits
 */
unsigned long long dimensions_capacity(struct PageFormat *format) {
    struct PageConstants constants;
    compute_constants(&constants, format);

    return constants.totalbits;
}