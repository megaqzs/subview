#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define __USE_GNU
#include <sys/mman.h>
#include "config.h"
#include <wayland-client.h>

#include "zwlr-shell.h"
#include "wayland.h"
#include "config.h"

#define MAP_SIZE WIDTH*HEIGHT*4

void *start_wayland_backend(void *);
void stop_wayland_backend(void);

typedef struct {
	struct wl_output *wl_output;
	struct zwlr_layer_surface_v1 *wlr_surf;
	struct wl_surface *surface;
	int32_t scale;
	uint32_t wl_name;
	struct wl_list link;
} output_t;

static void free_output(output_t *);
static void layer_surface_configure_handler(void *, struct zwlr_layer_surface_v1 *,
                                            uint32_t, uint32_t, uint32_t);
static void layer_surface_remove_handler(void *, struct zwlr_layer_surface_v1 *);
static void global_handler(void *, struct wl_registry *, uint32_t, const char *,
                           uint32_t);
static void global_remove_handler(void *, struct wl_registry *, uint32_t);
static void output_scale(void *, struct wl_output *, int32_t);
static void output_geometry(void *, struct wl_output *, int32_t, int32_t, int32_t,
                            int32_t, int32_t, const char *, const char *, int32_t);
static void output_mode(void *, struct wl_output *, uint32_t, int32_t, int32_t, int32_t);
static void output_done(void *, struct wl_output *);


static const struct zwlr_layer_surface_v1_listener layer_shell_listener = {
	.configure = layer_surface_configure_handler,
	.closed = layer_surface_remove_handler
};

static const struct wl_output_listener output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale,
};

static const struct wl_registry_listener registry_listener = {
	.global = global_handler,
	.global_remove = global_remove_handler
};

static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct zwlr_layer_shell_v1 *layer_shell;
struct wl_list outputs;
static struct wl_buffer *buffer;
static uint8_t *mapping;
static struct wl_shm_pool *pool;

static void free_output(output_t *output) {
    if (output->wlr_surf) {
        zwlr_layer_surface_v1_destroy(output->wlr_surf);
    }
    if (output->wl_output) {
        wl_output_destroy(output->wl_output);
    }
    if (output->surface) {
        wl_surface_destroy(output->surface);
    }

    wl_list_remove(&output->link);
    free(output);
}

static void layer_surface_configure_handler(
  void *data,
  struct zwlr_layer_surface_v1 *surface,
  uint32_t serial,
  uint32_t width,
  uint32_t height) {
	printf("configure event: w=%u, h=%u, surf=%p, s=%u\n", width, height, surface, serial);
	if (!(width == 0 || width == WIDTH) || !(height == 0 || height == HEIGHT))
		puts("ERROR: width or height are incorrect");
	zwlr_layer_surface_v1_ack_configure(surface, serial); // TODO: handle
}

static void layer_surface_remove_handler(void *data, struct zwlr_layer_surface_v1 *surface) {
	output_t *output;
	wl_list_for_each(output, &outputs, link) {
		if (output->wlr_surf == surface) {
			free_output(output);
			break;
		}
	}
}

static void output_geometry(void *data, struct wl_output *output, int32_t x,
							int32_t y, int32_t width_mm, int32_t height_mm, int32_t subpixel,
							const char *make, const char *model, int32_t transform) {
}

static void output_mode(void *data, struct wl_output *output, uint32_t flags,
						int32_t width, int32_t height, int32_t refresh) {
}

static void output_done(void *data, struct wl_output *wl_output)
{
	output_t *output = data;
	int fd;

	if (!output->wlr_surf) {
		output->surface = wl_compositor_create_surface(compositor);
		
		// Empty input region
		struct wl_region *input_region =
			wl_compositor_create_region(compositor);
		wl_surface_set_input_region(output->surface, input_region);
		wl_region_destroy(input_region);
		
		output->wlr_surf = zwlr_layer_shell_v1_get_layer_surface(layer_shell, output->surface, wl_output, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "subtitles");
		zwlr_layer_surface_v1_add_listener(output->wlr_surf, &layer_shell_listener, NULL);
		zwlr_layer_surface_v1_set_size(output->wlr_surf, WIDTH, HEIGHT);
		zwlr_layer_surface_v1_set_anchor(output->wlr_surf, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
		zwlr_layer_surface_v1_set_margin(output->wlr_surf, MARGIN_TOP, MARGIN_RIGHT, MARGIN_BOTTOM, MARGIN_LEFT);
		zwlr_layer_surface_v1_set_keyboard_interactivity(output->wlr_surf, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
		wl_surface_commit(output->surface);
		wl_display_roundtrip(display);
	
	
		wl_surface_attach(output->surface, buffer, 0, 0);
		wl_surface_commit(output->surface);

	}
}


static void output_scale(void *data, struct wl_output *wl_output, int32_t scale)
{
	output_t *output = data;
	output->scale = scale;
}

static void global_handler(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	struct wl_output_ *output;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		compositor = wl_registry_bind(registry, name,
			&wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		shm = wl_registry_bind(registry, name,
			&wl_shm_interface, 1);
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		layer_shell = wl_registry_bind(registry, name,
			&zwlr_layer_shell_v1_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		output_t *output = calloc(1, sizeof(output_t));
		output->wl_output = wl_registry_bind(registry, name,
			&wl_output_interface, 2);
		output->wl_name = name;
		wl_output_add_listener(output->wl_output, &output_listener, output);
		wl_list_insert(&outputs, &output->link);
	}
}

static void global_remove_handler(void *data, struct wl_registry *registry, uint32_t name) {
	output_t *output;
	wl_list_for_each(output, &outputs, link) {
		if (output->wl_name == name) {
			free_output(output);
			break;
		}
	}
} // TODO: handle


void stop_wayland_backend(void) {
	output_t *output, *tmp;
	wl_list_for_each_safe(output, tmp, &outputs, link)
		free_output(output);

	if (mapping)
		munmap(mapping, MAP_SIZE);
    if (pool)
        wl_shm_pool_destroy(pool);
	if (shm)
		wl_shm_destroy(shm);
	if (compositor)
		wl_compositor_destroy(compositor);
	if (layer_shell)
		zwlr_layer_shell_v1_destroy(layer_shell);
	if (registry)
		wl_registry_destroy(registry);
	if (display)
		wl_display_disconnect(display);
}

void *start_wayland_backend(void *arg) {
	int fd, ret = 0;
	display = wl_display_connect(NULL);

	wl_list_init(&outputs);

	if (display == NULL) {
		puts("ERROR: failed to connect to compositor");
		ret = -1;
		goto deallocate;
	}
	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display); // wait for the "initial" set of globals to appear


	// all our objects should be ready!
	if (!compositor || !shm || !layer_shell) {
		printf("Some required globals weren't found\n");
		ret = -1;
		goto deallocate;
	}

	fd = memfd_create("buffer", 0);
	ftruncate(fd, MAP_SIZE);
	if (!(mapping = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0))) {
		puts("ERROR: failed to map memory");
	}
	pool = wl_shm_create_pool(shm, fd, MAP_SIZE);
	close(fd);
	buffer = wl_shm_pool_create_buffer(pool, 0, WIDTH, HEIGHT, 4*WIDTH, WL_SHM_FORMAT_ARGB8888);
	
	if (arg)
		*((uint8_t**)arg) = mapping;
	else
		memset(mapping, 0x80, MAP_SIZE);

	while (wl_display_dispatch(display) != -1) {
	}
	puts("ERROR: failed to dispatch events");

deallocate:
	return NULL;
}