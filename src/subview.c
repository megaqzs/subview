#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "config.h"
#include "wayland/wayland.h"

#define MAP_SIZE WIDTH*HEIGHT*4

int main(int argc, char *argv[]) {
    char *line = NULL;
    size_t len = 0, ilen;
    if (!start_wayland_backend()) {
        while ((ilen = getline(&line, &len, stdin)) > 0) {
            if (ilen > 1 && line[ilen-1] == '\n') {
                line[--ilen] = '\0';
                if (ilen > 1 && line[ilen-1] == '\r')
                    line[--ilen] = '\0';
            }
            if (!ilen)
                break;

            pthread_mutex_lock(&txt_buf_lock);
            free(inp);
            inp = malloc(ilen+1);
            memcpy(inp, line, ilen+1);
            update_output();
            pthread_mutex_unlock(&txt_buf_lock);
        }
        free(line);
        stop_wayland_backend();
        pthread_mutex_lock(&txt_buf_lock);
        free(inp);
        inp = NULL;
        pthread_mutex_unlock(&txt_buf_lock);
    }
}
