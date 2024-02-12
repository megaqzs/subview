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
	char *line;
	size_t len = 0;
	pthread_t inf;
	if (!pthread_create(&inf, NULL, start_wayland_backend, NULL)) {
		while (getline(&line, &len, stdin) > 0) {
			int ilen = strlen(line);
			if (line[ilen-1] == '\n' || line[ilen-1] == '\r') {
				line[--ilen] = '\0';
				if (line[ilen-1] == '\n' || line[ilen-1] == '\r')
					line[--ilen] = '\0';
			}

			pthread_mutex_lock(&txt_buf_lock);
			free(inp);
			inp = malloc(ilen+1);
			memcpy(inp, line, ilen+1);
			update_output();
			pthread_mutex_unlock(&txt_buf_lock);
		}
		free(line);
		free(inp);
		stop_wayland_backend();
	}
		
}
