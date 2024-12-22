// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

// arg.c - standardised commandline argument handling

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arg.h"

// pro tier o(n) search
struct ArgHandle *arg_fromname(unsigned int handleslen, struct ArgHandle **handles, char *name) {
    for(int i = 0; i < handleslen; i++) {
        struct ArgHandle *handle = handles[i];
        if(!strcmp(handle->name, name)) {
            return handle;
        }
    }

    return NULL;
}

struct ArgHandle *arg_fromshortname(unsigned int handleslen, struct ArgHandle **handles, char shortname) {
    for(int i = 0; i < handleslen; i++) {
        struct ArgHandle *handle = handles[i];
        if(handle->shortname == shortname) {
            return handle;
        }
    }

    return NULL;
}

/**
 * 0 success
 * -1 generic fail
 * -2 not enough args to fufill overflowargs (should show help)
 * -3 too many overflow args
 * -4 not enough overflow args
 * -1xx invalid argument at argv index xx
 * -2xx argument at argv index xx is missing data field
 */
int arg_parse(unsigned int handleslen, struct ArgHandle **handles, unsigned int overflowlen, char **overflowout, int argc, char *argv[]) {
    if(argc < overflowlen + 1) return -2;

    int overflowindex = 0;

    for(int i = 1; i < argc; i++) {
        int more = i + 1 < argc;

        char *arg = argv[i];
        
        if(strlen(arg) > 1 && arg[0] == '-') { // is an argument
            struct ArgHandle *handle = NULL;
            if(arg[1] == '-' && strlen(arg) > 2) { // full length name
                char *argname = arg + 2; // remove starting "--"
                handle = arg_fromname(handleslen, handles, argname);
            } else if(strlen(arg) == 2) {
                char argcode = arg[1];
                handle = arg_fromshortname(handleslen, handles, argcode);
            }
            if(!handle) return -100 - i;

            if(handle->datafield) {
                if(!more) return -200 - i;

                char *morecontent = argv[++i];
                if(morecontent[0] == '-' && morecontent[1] == '-') return -200 - i; // it's probably missing the data field

                handle->handlearg(morecontent);
            } else {
                handle->handlearg(NULL);
            }
        } else if(strlen(arg) > 0) { // is not an argument, treat as overflow
            if(overflowindex == overflowlen) { // whoops! we're over, something has clearly gone wrong
                return -3;
            }

            overflowout[overflowindex] = arg;
            overflowindex++;
        } else {
            // empty string, just ignore
        }
    }

    if(overflowindex < overflowlen) { // we never got the full overflow requirement
        return -4;
    }

    return 0;
}