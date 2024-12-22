// Copyright (c) GPL 2024 Arkanic <https://github.com/Arkanic>

/* struct for representing argument handling */
struct ArgHandle {
    char *name;
    char shortname;

    short datafield;

    void (*handlearg)(char *data);
};

/**
 * 0 success
 * -1 generic fail
 * -2 not enough args to fufill overflowargs (should show help)
 * -3 too many overflow args
 * -1xx invalid argument at argv index xx
 * -2xx argument at argv index xx is missing data field
 */
int arg_parse(unsigned int handleslen, struct ArgHandle **handles, unsigned int overflowlen, char **overflowout, int argc, char *argv[]);