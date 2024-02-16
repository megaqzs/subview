#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "options.h"
#include "utils.h"
#include "config.h"

enum verbosity log_level = LOG_LEVEL;

void print_help(char *name) {
    printf(
        "Usage: %s [options]\n"
        "This program renders subtitles passed into it, and displays them as an overlay\n"
        "options:\n"
        "  -h          Show this help and exit\n"
        "  -V          Show program version and exit\n"
        "  -v          set verbosity level of the program (default " STRINGIFY(LOG_LEVEL) ")\n"
        "                 DEBUG: print everything\n"
        "                 INFO: print usefull information\n"
        "                 WARN: print about problems\n"
        "                 ERROR: only print when the program can't continue\n",
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
                if (!strcmp(optarg, "DEBUG"))
                    log_level = DEBUG;
                else if (!strcmp(optarg, "INFO"))
                    log_level = INFO;
                else if (!strcmp(optarg, "WARN"))
                    log_level = WARN;
                else if (!strcmp(optarg, "ERROR"))
                    log_level = ERROR;
                else {
                    print_help(argc ? argv[0] : PROGNAME);
                    return -3;
                }
                break;
            case -1:
                parsing = 0;
                break;
        }
    }
    return 0;
}
