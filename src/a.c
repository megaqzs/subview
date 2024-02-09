#include <wayland-client.h>
#include <cairo/cairo.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define __USE_GNU
#include <sys/mman.h>

#include "zwlr-shell.h"

struct wl_compositor *compositor;
struct wl_shm *shm;
struct zwlr_layer_shell_v1 *layer_shell;

void registry_global_handler(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = (struct wl_compositor *) wl_registry_bind(registry, name,
            &wl_compositor_interface, 3);
    } else if (strcmp(interface, "wl_shm") == 0) {
        shm = (struct wl_shm *) wl_registry_bind(registry, name,
            &wl_shm_interface, 1);
    } else if (strcmp(interface, "zwlr_layer_shell_v1") == 0) {
        layer_shell = (struct zwlr_layer_shell_v1 *) wl_registry_bind(registry, name,
            &zwlr_layer_shell_v1_interface, 1);
	}
}

void registry_global_remove_handler(void *data, struct wl_registry *registry, uint32_t name) {
} // TODO: handle

const struct wl_registry_listener registry_listener = {
    .global = registry_global_handler,
    .global_remove = registry_global_remove_handler
};

void layer_surface_configure_handler(
  void *data,
  struct zwlr_layer_surface_v1 *surface,
  uint32_t serial,
  uint32_t width,
  uint32_t height) {
	printf("configure event: w=%u, h=%u, surf=%p, s=%u\n", width, height, surface, serial);
	zwlr_layer_surface_v1_ack_configure(surface, serial); // TODO: handle
}

void layer_surface_remove_handler(void *data, struct zwlr_layer_surface_v1 *surface) {
	puts("unhandeled remove"); // TODO: handle
}

const struct zwlr_layer_surface_v1_listener layer_shell_listener = {
    .configure = layer_surface_configure_handler,
    .closed = layer_surface_remove_handler
};

#define WIDTH 200
#define HEIGHT 200
#define MAP_SIZE WIDTH*HEIGHT*4
int main(int argc, char *argv[]) {
	int ret = 0;
	int fd;
	unsigned char *mapping;
	struct zwlr_layer_surface_v1 *wlr_surf;
	struct wl_surface *surface;
	struct wl_buffer *buffer;
	struct wl_registry *registry;
	struct wl_display *display = wl_display_connect(NULL);
	struct wl_shm_pool *pool;

	if (display == NULL) {
		puts("ERROR: failed to connect to compositor");
		ret = -1;
		goto deallocate;
	}
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    // wait for the "initial" set of globals to appear
    wl_display_roundtrip(display);


    // all our objects should be ready!
    if (compositor && shm && layer_shell) {
        printf("Got them all!\n");
    } else {
        printf("Some required globals unavailable\n");
		ret = -1;
    }

	surface = wl_compositor_create_surface(compositor);
	wlr_surf = zwlr_layer_shell_v1_get_layer_surface(
			layer_shell,
			surface,
			NULL,
			ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
			"subtitles");
	zwlr_layer_surface_v1_add_listener(wlr_surf, &layer_shell_listener, NULL);
	zwlr_layer_surface_v1_set_size(wlr_surf, WIDTH, HEIGHT);
	zwlr_layer_surface_v1_set_anchor(wlr_surf, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
	wl_surface_commit(surface);

	fd = memfd_create("buffer", 0);
	ftruncate(fd, MAP_SIZE);
	mapping = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	pool = wl_shm_create_pool(shm, fd, MAP_SIZE);
	close(fd);
	memset(mapping, 0x80, MAP_SIZE);
	buffer = wl_shm_pool_create_buffer(pool, 0, WIDTH, HEIGHT, 4*WIDTH, WL_SHM_FORMAT_ARGB8888);

	wl_display_roundtrip(display);

	wl_surface_attach(surface, buffer, 0, 0);
	wl_surface_commit(surface);
	wl_display_roundtrip(display);

	printf("wlr_surf = %p, surf = %p\n", wlr_surf, surface);
	getc(stdin);

	deallocate:
	zwlr_layer_surface_v1_destroy(wlr_surf);
	wl_surface_destroy(surface);
	wl_buffer_destroy(buffer);
	wl_shm_pool_destroy(pool);
	wl_registry_destroy(registry);

	wl_display_disconnect(display);
	munmap(mapping, MAP_SIZE);
	return ret;
}
