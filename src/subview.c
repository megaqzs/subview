#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "wayland/wayland.h"
#include "options.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    char *line = NULL;
    ssize_t ilen;
    size_t len = 0;
    options_t *options = malloc(sizeof(*options));
    if (!parse_args(argc, argv, options) && !start_wayland_backend(options)) {
        while ((ilen = getline(&line, &len, stdin)) > 0) {
            if (ilen > 0 && line[ilen-1] == '\n') {
                line[--ilen] = '\0';
                if (ilen > 0 && line[ilen-1] == '\r')
                    line[--ilen] = '\0';
            }
            PDEBUG("line length is %ld", ilen);
            if (!ilen)
                break;

            pthread_mutex_lock(&txt_buf_lock);
            free(inp);
            inp = malloc(ilen+1);
            memcpy(inp, line, ilen+1);
            update_output();
            pthread_mutex_unlock(&txt_buf_lock);
        }
        stop_wayland_backend();
        free(line);
        free(inp);
    }
}
