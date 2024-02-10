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
	pthread_t inf;
	if (!pthread_create(&inf, NULL, start_wayland_backend, NULL)) {
		pthread_mutex_lock(&txt_buf_lock);
		while (fgets(inp, sizeof(inp), stdin)) {
				inp[strlen(inp)-1] = '\0';
				pthread_mutex_unlock(&txt_buf_lock);
				update_output();
				pthread_mutex_lock(&txt_buf_lock);
		}
		pthread_mutex_unlock(&txt_buf_lock);
		stop_wayland_backend();
	}
		
}
