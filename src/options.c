#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "options.h"
#include "utils.h"
#include "config.h"

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

void print_help(void) {
    printf(
        "Usage: %s [options]\n"
        "This program renders subtitles passed into it, and displays them as an overlay\n"
        "options:\n"
        "  -h          Show this help and exit\n"
        "  -f          forcefully open the control socket, even if it already exists\n"
        "  -p          print the control socket path\n"
        "  -d          daemonise the program after creating control socket\n"
        "  -l <path>   print log messages to the file specified by path, replaces stderr if daemonising before\n"
        "  -s <path>   set the path to the control socket\n"
        "  -V          Show program version and exit\n"
        "  -v <level>  set verbosity level of the program (default " STRINGIFY(LOG_LEVEL) ")\n"
        "                 3, DEBUG: print everything\n"
        "                 2, INFO: print usefull information\n"
        "                 1, WARN: print about problems\n"
        "                 0, ERROR: only print when the program can't continue\n",
        progname);
}

int parse_args(int argc, char *argv[], options_t *options) {
    progname = argc ? argv[0] : progname; // use first argument as program name if available
    log_files[0] = stderr;
    int parsing = 1;
    memcpy(options, &default_options, sizeof(*options));
    while (parsing) {
        switch (getopt(argc, argv, "hfs:pdVv:l:")) {
            case '?':
            case 'h':
                print_help();
                return -1;
            case 'V':
                printf("%s version %s\n", progname, version);
                return -2;
            case 'p':
                options->print_path = true;
                break;
            case 'f':
                options->force = true;
                break;
            case 'd':
                options->daemonise = true;
                break;
            case 'l':
                int i = 0;
                if (options->daemonise && log_files[0] == stderr) {
                    log_files[0] = fopen(optarg, "a");
                    break;
                }
                while (log_files[i] != NULL && ++i < MAX_LOG_FILES) {
                }
                if (i >= MAX_LOG_FILES) {
                    PERROR("Too many log files");
                    return -3;
                }
                log_files[i] = fopen(optarg, "a");
                break;
            case 's':
                options->path = optarg;
                break;
            case 'v':
                char *endptr;
                log_level = strtol(optarg, &endptr, 10);
                if (!*endptr)
                    break; // no need to continue checking since we found the log level
                if (!strcasecmp(optarg, "DEBUG"))
                    log_level = DEBUG;
                else if (!strcasecmp(optarg, "INFO"))
                    log_level = INFO;
                else if (!strcasecmp(optarg, "WARN"))
                    log_level = WARN;
                else if (!strcasecmp(optarg, "ERROR"))
                    log_level = ERROR;
                else {
                    print_help();
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
