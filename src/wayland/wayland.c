#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define __USE_GNU
#include <sys/mman.h>
#include <wayland-client.h>
#include <stdbool.h>

#include "zwlr-shell.h"
#include "options.h"
#include "pangocairo/renderer.h"
#include "utils.h"



void update_output(void);
void stop_wayland_backend(void);
int start_wayland_backend(options_t *);

typedef struct {
    struct wl_output *wl_output;
    struct zwlr_layer_surface_v1 *wlr_surf;
    struct wl_surface *surface;
    struct wl_callback *cb;
    int32_t scale;
    uint32_t wl_name;
    _Atomic bool new_data;
    struct wl_list link;
    options_t options;
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

static void frame_done(void *, struct wl_callback *, uint32_t);
static struct wl_buffer *draw_frame(output_t *output);
static int allocate_shm_file(size_t);
static void buffer_release(void *, struct wl_buffer *);
static void free_backend(void);
static pthread_t backend;
static void *_start_wayland_backend(void *);

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
static const struct wl_callback_listener frame_listener = {
    .done = frame_done
};
static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release
};

pthread_mutex_t txt_buf_lock;
char *inp;
_Atomic bool closed;
static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct zwlr_layer_shell_v1 *layer_shell;
static pthread_mutex_t outputs_lock;
static struct wl_list outputs;
static struct wl_buffer *buffer;


static void free_output(output_t *output) {
    PDEBUG("output %u is being freed", output->wl_name);
    if (output->wlr_surf) {
        zwlr_layer_surface_v1_destroy(output->wlr_surf);
    }
    if (output->wl_output) {
        wl_output_destroy(output->wl_output);
    }
    if (output->surface) {
        wl_surface_destroy(output->surface);
    }
    if (output->cb) {
        wl_callback_destroy(output->cb);
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
    output_t *output = data;

    PDEBUG("configure event triggered");
    if (!(width == 0 || width == output->options.width) || !(height == 0 || height == output->options.height))
        PWARN("width or height are incorrect, expected %ux%u, but got %ux%u, ignoring",
                output->options.width, output->options.height, width, height); // TODO: handle

    zwlr_layer_surface_v1_ack_configure(surface, serial);

    buffer = draw_frame(output);
    wl_surface_attach(output->surface, buffer, 0, 0);
    wl_surface_commit(output->surface);


}

static void layer_surface_remove_handler(void *data, struct zwlr_layer_surface_v1 *surface) {
    output_t *output, *tmp;
    pthread_mutex_lock(&outputs_lock);
    wl_list_for_each_safe(output, tmp, &outputs, link) {
        if (output->wlr_surf == surface) {
            free_output(output);
            break;
        }
    }
    pthread_mutex_unlock(&outputs_lock);
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
    options_t *options = &output->options;
    int fd;

    PDEBUG("configuring output %u", output->wl_name);
    if (!output->wlr_surf) {
        output->surface = wl_compositor_create_surface(compositor);
        
        // Empty input region
        struct wl_region *input_region =
            wl_compositor_create_region(compositor);
        wl_surface_set_input_region(output->surface, input_region);
        wl_region_destroy(input_region);
        
        output->wlr_surf = zwlr_layer_shell_v1_get_layer_surface(layer_shell, output->surface, wl_output, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "subtitles");
        zwlr_layer_surface_v1_add_listener(output->wlr_surf, &layer_shell_listener, output);
        zwlr_layer_surface_v1_set_size(output->wlr_surf, options->width, options->height);
        zwlr_layer_surface_v1_set_anchor(output->wlr_surf, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
        zwlr_layer_surface_v1_set_margin(output->wlr_surf, options->margin_top, options->margin_right, options->margin_bottom, options->margin_left);
        zwlr_layer_surface_v1_set_keyboard_interactivity(output->wlr_surf, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    
        output->cb = wl_surface_frame(output->surface);
        wl_callback_add_listener(output->cb, &frame_listener, output);
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
        PDEBUG("registering compositor interface");
        compositor = wl_registry_bind(registry, name,
            &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        PDEBUG("registering shm interface");
        shm = wl_registry_bind(registry, name,
            &wl_shm_interface, 1);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        PDEBUG("registering layer shell interface");
        layer_shell = wl_registry_bind(registry, name,
            &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        PDEBUG("registering output interface");
        output_t *output = calloc(1, sizeof(output_t));
        output->wl_output = wl_registry_bind(registry, name,
            &wl_output_interface, 2);
        output->wl_name = name;
        memcpy(&output->options, data, sizeof(output->options));
        wl_output_add_listener(output->wl_output, &output_listener, output);
        pthread_mutex_lock(&outputs_lock);
        wl_list_insert(&outputs, &output->link);
        pthread_mutex_unlock(&outputs_lock);
    }
}

static void global_remove_handler(void *data, struct wl_registry *registry, uint32_t name) {
    output_t *output, *tmp;
    pthread_mutex_lock(&outputs_lock);
    wl_list_for_each_safe(output, tmp, &outputs, link) {
        if (output->wl_name == name) {
            free_output(output);
            break;
        }
    }
    pthread_mutex_unlock(&outputs_lock);
}

static int allocate_shm_file(size_t size) {
    int fd = memfd_create("buffer", 0);
    ftruncate(fd, size);
    return fd;
}

static struct wl_buffer *draw_frame(output_t *output)
{
    int fd;
    options_t *options = &output->options;
    struct wl_shm_pool *pool;
    uint32_t stride, size;

    PDEBUG("drawing frame");

    stride = get_surf_stride(options);
    size = stride*options->height;
    fd = allocate_shm_file(size);
    if (fd == -1) {
        return NULL;
    }

    char *data = mmap(NULL, size,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }


    pool = wl_shm_create_pool(shm, fd, size);
    buffer = wl_shm_pool_create_buffer(pool, 0,
            options->width, options->height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    if (inp) {
        PDEBUG("got input, writing to surface");
        pthread_mutex_lock(&txt_buf_lock);
        draw_text(inp, data, stride, options);
        pthread_mutex_unlock(&txt_buf_lock);
    }

    munmap(data, size);
    wl_buffer_add_listener(buffer, &buffer_listener, NULL);
    return buffer;
}

static void buffer_release(void *data, struct wl_buffer *buffer) {
    wl_buffer_destroy(buffer);
}

static void frame_done(void *data, struct wl_callback *cb, uint32_t time) {
    if (cb)
        wl_callback_destroy(cb);
    output_t *output = data;

    output->cb = wl_surface_frame(output->surface);
    wl_callback_add_listener(output->cb, &frame_listener, output);

    /* Submit a frame for this event */
    if (output->new_data) {
        struct wl_buffer *buffer = draw_frame(output);
        wl_surface_attach(output->surface, buffer, 0, 0);
        wl_surface_damage_buffer(output->surface, 0, 0, INT32_MAX, INT32_MAX);
        output->new_data = false;
    }
    wl_surface_commit(output->surface);
}

// needs to be executed in the backend thread if it's running
static void free_backend(void) {
    output_t *output, *tmp;
    PDEBUG("freeing backend");
    wl_list_for_each_safe(output, tmp, &outputs, link)
        free_output(output);
    if (shm) {
        wl_shm_destroy(shm);
        shm = NULL;
    }
    if (compositor) {
        wl_compositor_destroy(compositor);
        compositor = NULL;
    }
    if (layer_shell) {
        zwlr_layer_shell_v1_destroy(layer_shell);
        layer_shell = NULL;
    }
    if (registry) {
        wl_registry_destroy(registry);
        registry = NULL;
    }
    if (display) {
        wl_display_disconnect(display);
        display = NULL;
    }
    pthread_mutex_destroy(&txt_buf_lock);
    pthread_mutex_destroy(&outputs_lock);
}

// backend thread function
static void *_start_wayland_backend(void *arg) {
    int status;
    while (!closed && (status = wl_display_dispatch(display)) != -1) {
    } // only works because of frame events that make wl_display_dispatch not block forever for sure
    // TODO: make it work without frame events, in case of no output
    if (status == -1)
        puts("ERROR: failed to dispatch events");
    free_backend();
}

void update_output(void) {
    output_t *output;

    pthread_mutex_lock(&outputs_lock);
    wl_list_for_each(output, &outputs, link) {
        output->new_data = true;
    }
    pthread_mutex_unlock(&outputs_lock);
}

void stop_wayland_backend(void) {
    closed = true;
    pthread_join(backend, NULL);
}

int start_wayland_backend(options_t *options) {
    pthread_mutex_init(&outputs_lock, NULL);
    pthread_mutex_init(&txt_buf_lock, NULL);

    wl_list_init(&outputs);
    display = wl_display_connect(NULL);

    if (display == NULL) {
        PERROR("failed to connect to compositor");
        free_backend();
        return -3;
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, options);
    wl_display_roundtrip(display); // wait for the "initial" set of globals to appear

    // all our objects should be ready!
    if (!compositor || !shm || !layer_shell) {
        PERROR("some wayland globals weren't found [%p, %p, %p]", compositor, shm, layer_shell);
        free_backend();
        return -2;
    }

    if (!pthread_create(&backend, NULL, _start_wayland_backend, NULL))
        return 0;

    free_backend();
    return -1;
}
