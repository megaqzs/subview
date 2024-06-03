#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "options.h"
#include "utils.h"
#include "config.h"

enum verbosity log_level = LOG_LEVEL;
FILE *log_files[MAX_LOG_FILES];

void _logger(enum verbosity lvl, char *format, ...) {
    va_list args;
    va_start(args, format);
    if (log_level >= lvl) {
        for (int i = 0; i < MAX_LOG_FILES; i++) {
            if (log_files[i] != NULL)
                vfprintf(log_files[i], format, args);
        }
    }
    va_end(args);
}

void print_help(char *name) {
    printf(
        "Usage: %s [options]\n"
        "This program renders subtitles passed into it, and displays them as an overlay\n"
        "options:\n"
        "  -h          Show this help and exit\n"
        "  -p          print the control socket path\n"
        "  -d          daemonise the program after creating control socket\n"
        "  -V          Show program version and exit\n"
        "  -v          set verbosity level of the program (default " STRINGIFY(LOG_LEVEL) ")\n"
        "                 3, DEBUG: print everything\n"
        "                 2, INFO: print usefull information\n"
        "                 1, WARN: print about problems\n"
        "                 0, ERROR: only print when the program can't continue\n",
        name);
}

int parse_args(int argc, char *argv[], options_t *options) {
    log_files[0] = stderr;
    enum verbosity curr_level;
    int parsing = 1;
    memcpy(options, &default_options, sizeof(*options));
    while (parsing) {
        switch (getopt(argc, argv, "hpdVv:")) {
            case '?':
            case 'h':
                print_help(argc ? argv[0] : PROGNAME);
                return -1;
            case 'V':
                printf("%s version " VERSION "\n", argc ? argv[0] : PROGNAME);
                return -2;
            case 'p':
                options->print_path = true;
                break;
            case 'd':
                options->daemonise = true;
                break;
            case 'v':
                char *endptr;
                curr_level = strtol(optarg, &endptr, 10);
                if (!*endptr)
                    break; // no need to continue checking since we found the log level
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
    log_level = curr_level;
    return 0;
}
