#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "options.h"
#include "config.h"


void print_help(char *name) {
    printf(
        "Usage: %s [options]\n"
        "This program renders subtitles passed into it, and displays them as an overlay\n"
        "options:\n"
        "  -h          Show this help and exit\n"
        "  -V          Show program version and exit\n", 
        name);
}

int parse_args(int argc, char *argv[], options_t *options) {
    int parsing = 1;
    memcpy(options, &default_options, sizeof(*options));
    while (parsing) {
        switch (getopt(argc, argv, "hVv:")) {
            case '?':
            case 'h':
                print_help(argc ? argv[0] : PROGNAME);
                return -1;
            case 'V':
                printf("%s version " VERSION "\n", argc ? argv[0] : PROGNAME);
                return -2;
            case 'v':
                options->level = 0;
                break;
            case -1:
                parsing = 0;
                break;
        }
    }
    return 0;
}
