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
	size_t len = 0;
	pthread_t inf;
	if (!pthread_create(&inf, NULL, start_wayland_backend, NULL)) {
		pthread_mutex_lock(&txt_buf_lock);
		while (getline(&inp, &len, stdin) > 0) {
			int ilen = strlen(inp)-1;
			if (inp[ilen] == '\n' || inp[ilen] == '\r') {
				inp[ilen--] = '\0';
				if (inp[ilen] == '\n' || inp[ilen] == '\r')
					inp[ilen--] = '\0';
			}

			pthread_mutex_unlock(&txt_buf_lock);
			update_output();
			pthread_mutex_lock(&txt_buf_lock);
		}
		free(inp);
		stop_wayland_backend();
	}
		
}
