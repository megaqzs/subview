#include <cairo/cairo.h>
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
		while (scanf("%08x", &inp)) {
				update_output();

			while (getc(stdin) != '\n');
		}
		stop_wayland_backend();
	}
		
}
