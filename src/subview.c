#include <cairo/cairo.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "config.h"
#include "wayland/wayland.h"

int main(int argc, char *argv[]) {
	pthread_t inf;
	uint8_t *mapping;
	if (!pthread_create(&inf, NULL, start_wayland_backend, (void*) NULL)) {
		getc(stdin);
		stop_wayland_backend();
	}
		
}
