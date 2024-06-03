#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>

#define __USE_GNU
#include <sys/mman.h>
#include <wayland-client.h>
#include <stdbool.h>
#include <poll.h>

#include "zwlr-shell.h"
#include "options.h"
#include "connection.h"
#include "pangocairo/renderer.h"
#include "utils.h"

#define INITIAL_CONNLIST_SIZE 50

int start_wayland_backend(options_t *, int);
_Atomic bool closed = true;

typedef struct {
    int updcount;
    pthread_t thread;
    connection_t *connection;
    options_t options;
    struct wl_list link;
} region_t;

// container type for output specific data
typedef struct {
    struct wl_output *wl_output;
    struct zwlr_layer_surface_v1 *wlr_surf;
    struct wl_surface *surface;
    struct wl_callback *cb;
    int32_t scale;
    bool regupd;
    bool new_data;
    size_t regions_size;
    uint32_t wl_name;
    struct wl_list regions;
    struct wl_list link;
    options_t options;
} output_t;

enum {
    WAYLAND_POLLFD,
    CONTROL_POLLFD,
    CONNLIST_START,
};

static region_t *new_region(output_t *, connection_t *);
static void free_region(output_t *, region_t *);
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
static void event_loop();

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

static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct zwlr_layer_shell_v1 *layer_shell;
static struct wl_list outputs;
static struct pollfd *fds;
static size_t nfds;
static size_t nfds_allocated;

// create region data for new connections
static region_t *new_region(output_t *output, connection_t *conn) {
    PDEBUG("region for fd %d is being allocated", conn->pfd->fd);
    region_t *region = malloc(sizeof(region_t));

    // update region count and set the region update flag
    output->regupd = true;
    output->regions_size++;

    region->options = output->options;
    region->updcount = 0;
    region->connection = conn;
    reftick_connection(conn);
    wl_list_insert(&output->regions, &region->link);

}

static void free_region(output_t *output, region_t *region) {
    PDEBUG("region for fd %d is being freed", region->connection->pfd->fd);
    output->regupd = true;
    output->regions_size--;
    refdrop_connection(region->connection);
    wl_list_remove(&region->link);
    free(region);
}

// free output related data in case of exit or output disconnection
static void free_output(output_t *output) {
    PINFO("output %u is being destroyed", output->wl_name);
    region_t *region, *tmp;
    wl_list_for_each_safe(region, tmp, &output->regions, link)
        free_region(output, region);
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
    struct wl_buffer *buffer;

    PDEBUG("configure event triggered");
    if (!(width == 0 || width == output->options.width) || !(height == 0 || height == output->options.height)) {
        PWARN("width or height are incorrect, expected %ux%u, but got %ux%u, ignoring",
                output->options.width, output->options.height, width, height); // TODO: handle
        if (width) output->options.width = width;
        if (height) output->options.height = height;
    }

    zwlr_layer_surface_v1_ack_configure(surface, serial);
    frame_done(output, NULL, -1); // draw surface using new dimensions
}

// handle surface destruction events by removing output
static void layer_surface_remove_handler(void *data, struct zwlr_layer_surface_v1 *surface) {
    output_t *output, *tmp;
    wl_list_for_each_safe(output, tmp, &outputs, link) {
        if (output->wlr_surf == surface) {
            free_output(output);
            break;
        }
    }
}

// event listener for the output geometry event
static void output_geometry(void *data, struct wl_output *output, int32_t x,
                            int32_t y, int32_t width_mm, int32_t height_mm, int32_t subpixel,
                            const char *make, const char *model, int32_t transform) {
}

// event listener for the output mode event
static void output_mode(void *data, struct wl_output *output, uint32_t flags,
                        int32_t width, int32_t height, int32_t refresh) {
}

// event listener for the output done event
static void output_done(void *data, struct wl_output *wl_output) {
    output_t *output = data;
    options_t *options = &output->options;

    PDEBUG("configuring output %u", output->wl_name);
    if (!output->wlr_surf) {
        output->surface = wl_compositor_create_surface(compositor);
        
        // Empty input region
        struct wl_region *input_region =
            wl_compositor_create_region(compositor);
        wl_surface_set_input_region(output->surface, input_region);
        wl_region_destroy(input_region);
        
        // attach surface to output based on options
        output->wlr_surf = zwlr_layer_shell_v1_get_layer_surface(layer_shell, output->surface, wl_output, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "subtitles");
        zwlr_layer_surface_v1_add_listener(output->wlr_surf, &layer_shell_listener, output);
        zwlr_layer_surface_v1_set_size(output->wlr_surf, options->width, options->height);
        zwlr_layer_surface_v1_set_anchor(output->wlr_surf, options->anchor);
        zwlr_layer_surface_v1_set_margin(output->wlr_surf, options->margin_top, options->margin_right, options->margin_bottom, options->margin_left);
        zwlr_layer_surface_v1_set_keyboard_interactivity(output->wlr_surf, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);

        output->cb = wl_surface_frame(output->surface);
        wl_callback_add_listener(output->cb, &frame_listener, output);
        wl_surface_commit(output->surface);
    }
}


// event listener for the output scale event
static void output_scale(void *data, struct wl_output *wl_output, int32_t scale)
{
    output_t *output = data;
    output->scale = scale;
}

// register wayland server interfaces and add event listeners if needed
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
        output->regions_size = 0;
        wl_list_init(&output->regions);
        output->wl_output = wl_registry_bind(registry, name,
            &wl_output_interface, 2);
        output->wl_name = name;
        memcpy(&output->options, data, sizeof(output->options));
        wl_output_add_listener(output->wl_output, &output_listener, output);
        wl_list_insert(&outputs, &output->link);
    }
}

// handle output destruction (by disconnecting it for example)
static void global_remove_handler(void *data, struct wl_registry *registry, uint32_t name) {
    output_t *output, *tmp;
    wl_list_for_each_safe(output, tmp, &outputs, link) {
        if (output->wl_name == name) {
            free_output(output);
            break;
        }
    }
}

static int allocate_shm_file(size_t size) {
    int fd = memfd_create("buffer", 0);
    ftruncate(fd, size);
    return fd;
}

static struct wl_buffer *draw_frame(output_t *output)
{
    int fd;
    region_t *region;
    options_t *options = &output->options;
    struct wl_shm_pool *pool;
    uint32_t stride, size;
    struct wl_buffer *buffer = NULL;

    PDEBUG("drawing frame");

    stride = get_surf_stride(options); // the stride is the distance between rows in bytes
    size = stride*options->height;
    fd = allocate_shm_file(size); // allocate the shared memory buffer as a file
    if (fd == -1) {
        PWARN("failed to create surface buffer file descriptor with error `%s`, continuing without drawing text", strerror(errno));
        return NULL;
    }

    // memory map the shared mapping between thread and compositor for sharing the surface buffer
    char *data = mmap(NULL, size,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        PWARN("failed to map surface buffer with error `%s`, continuing without drawing text", strerror(errno));
        close(fd);
        return NULL;
    }


    pool = wl_shm_create_pool(shm, fd, size);
    buffer = wl_shm_pool_create_buffer(pool, 0,
            options->width, options->height, stride, WL_SHM_FORMAT_ARGB8888);
    // the pool uses a fixed area which makes it hard to reuse
    wl_shm_pool_destroy(pool);
    close(fd); // the mapping keeps the shared buffer memory open

    if (output->regupd) {
        double y = 0;
        wl_list_for_each(region, &output->regions, link) {
            region->options.x = 0;
            region->options.y = y;
            region->options.width = options->width;
            y += (double) options->height / output->regions_size;
            region->options.height = y-region->options.y;
        }
    }
    wl_list_for_each(region, &output->regions, link) {
        if ((output->regupd || region->updcount != region->connection->updcount)) {
            struct draw_args *args = malloc(sizeof(struct draw_args));
            if (!args) {
                PWARN("failed to allocate memory for region drawing thread arguments, continuing without drawing region");
                continue;
            }
            args->text = region->connection->vis_buff; args->buffer = data; args->stride = stride; args->options = &region->options;
            pthread_create(&region->thread, NULL, draw_text, args);
        }

    }
    wl_list_for_each(region, &output->regions, link) {
        if ((output->regupd || region->updcount != region->connection->updcount)) {
            int err = pthread_join(region->thread, NULL);
            if (err)
                PWARN("failed to join thread with error `%s`, some regions might not be fully drawn", strerror(err));
        }
    }

    munmap(data, size); // no need for the surface buffer on this end anymore
    wl_buffer_add_listener(buffer, &buffer_listener, NULL); // add a callback for destroying the buffer after it has been used by the compositor
    return buffer;
}

static void buffer_release(void *data, struct wl_buffer *buffer) {
    wl_buffer_destroy(buffer);
}

static void frame_done(void *data, struct wl_callback *cb, uint32_t time) {
    region_t *region;
    struct wl_buffer *buffer;
    output_t *output = data;
    if (time != -1) {
        if (cb)
            wl_callback_destroy(cb);
        // add the callback listener in order to get notifications for when to draw frames
        output->cb = wl_surface_frame(output->surface);
        wl_callback_add_listener(output->cb, &frame_listener, output);
    }

    if ((time == -1 || output->regupd || output->new_data) && (buffer = draw_frame(output)) != NULL) {
        // if there is new data available we draw it and mark the entire surface as dirty so that all of it would be updated
        wl_surface_attach(output->surface, buffer, 0, 0);
        if (output->regupd)
            wl_surface_damage_buffer(output->surface, 0, 0, INT32_MAX, INT32_MAX); // mark the entire surface as dirty in case of region changes
        else if (output->new_data) {
            wl_list_for_each(region, &output->regions, link) { // mark all regions with new data as dirty
                options_t *opts = &region->options;
                if (region->updcount != region->connection->updcount) {
                    wl_surface_damage_buffer(output->surface, opts->x, opts->y, opts->width, opts->height);
                    region->updcount = region->connection->updcount;
                }
            }
        }
        output->new_data = output->regupd = false; // we have already read the data, so reset the new data flags
    }
    // in either case we commit to the surface in order to trigger callback, and draw the new surface if it exists
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
}

// inspired by https://gitlab.freedesktop.org/wayland/wayland/-/blob/main/src/wayland-client.c#L1999
static void *wayland_dispatch_start(struct wl_display *, struct pollfd *);
static void *wayland_dispatch_mid(struct wl_display *, struct pollfd *);
static void *wayland_dispatch_end(struct wl_display *, struct pollfd *);

typedef void* (*wayland_dispatch_t)(struct wl_display*, struct pollfd*);

static void *wayland_dispatch_start(struct wl_display *display, struct pollfd *fd) {
    if (!fd)
        return NULL;

    while (wl_display_prepare_read(display) == -1) {
        if (wl_display_dispatch_pending(display) < 0) {
            return NULL;
        }
    }
    fd->events = fd->revents = POLLOUT; // trick to make it think that there was a poll event before so that it can run
    return wayland_dispatch_mid(display, fd);
}

static void *wayland_dispatch_mid(struct wl_display *display, struct pollfd *fd) {
    int ret;
    if (!fd) {
        wl_display_cancel_read(display);
        return NULL;
    }

    if (fd->events && !fd->revents)
        return wayland_dispatch_mid; // pretend we didn't see anything if no events happened while we were expecting them

    ret = wl_display_flush(display);

    if (ret != -1 || errno != EAGAIN) {
        /* Don't stop if flushing hits an EPIPE; continue so we can read any
         * protocol error that may have triggered it. */
        if (ret < 0 && errno != EPIPE) {
            wl_display_cancel_read(display);
            return NULL;
        }
        fd->events = POLLIN;
        return wayland_dispatch_end;
    }

    return wayland_dispatch_mid;
}

static void *wayland_dispatch_end(struct wl_display *display, struct pollfd *fd) {
    if (!fd) {
        wl_display_cancel_read(display);
        return NULL;
    }

    if (fd->events && !fd->revents)
        return wayland_dispatch_end; // pretend we didn't see anything if no events happened while we were expecting them

    if (wl_display_read_events(display) == -1)
        return NULL;
    if (wl_display_dispatch_pending(display) == -1)
        return NULL;

    errno = EAGAIN;
    fd->events = 0;
    return NULL;
}

// event loop for socket connections and wayland
static void event_loop() {
    closed = false;
    // run for every event as long as closed wasn't set by anyone and there was no error in wl_display_dispatch
    wayland_dispatch_t wayland_dispatch = wayland_dispatch_start(display, &fds[WAYLAND_POLLFD]);

    while ((poll(fds, nfds, -1) > 0 || errno == EINTR || errno == EAGAIN) && !closed) {
        if (!(wayland_dispatch = wayland_dispatch(display, &fds[WAYLAND_POLLFD]))) {
            if (errno == EAGAIN)
                wayland_dispatch = wayland_dispatch_start(display, &fds[WAYLAND_POLLFD]);
            else
                break;
        }
        output_t *output;
        if (fds[CONTROL_POLLFD].revents != 0) { // TODO: handle errors
            if (fds[CONTROL_POLLFD].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                PERROR("control socket problem has occured");
                break;
            }
            int fd = accept(fds[CONTROL_POLLFD].fd, NULL, NULL);
            connection_t *conn = NULL;
            for (int i = 0; i < nfds; i++) {
                if (i >= nfds || fds[i].fd < 0) {
                    conn = new_connection(&fds[i], fd);
                    break;
                }
            }
            if (!conn && nfds < nfds_allocated)
                conn = new_connection(&fds[nfds++], fd);
            if (conn) {
                PDEBUG("new connection from fd %d", fd);
                wl_list_for_each(output, &outputs, link)
                    new_region(output, conn);
                refdrop_connection(conn); // drop reference since we are going out of the context of the reference
            }
        }

        wl_list_for_each(output, &outputs, link) {
            region_t *region, *tmp;
            wl_list_for_each_safe(region, tmp, &output->regions, link) {
                if (update_connection(region->connection) < 0)
                    free_region(output, region);
                else
                    output->new_data |= region->updcount != region->connection->updcount;
            }
        }
    }
    if (wayland_dispatch)
        wayland_dispatch(display, NULL); // cancel read if we are in the middle of reading

    PDEBUG("event loop end");
    if (wayland_dispatch == NULL && errno != EAGAIN)
        PERROR("failed to dispatch events");
    free_backend();
}

// starts the wayland event loop thread
int start_wayland_backend(options_t *options, int controlfd) {
    wl_list_init(&outputs);
    display = wl_display_connect(NULL);

    // initialize the fd list
    fds = calloc(CONNLIST_START+INITIAL_CONNLIST_SIZE, sizeof(struct pollfd));
    nfds_allocated = CONNLIST_START+INITIAL_CONNLIST_SIZE;
    nfds = CONNLIST_START;
    fds[WAYLAND_POLLFD].fd = wl_display_get_fd(display);
    fds[WAYLAND_POLLFD].events = 0;
    fds[CONTROL_POLLFD].fd = controlfd;
    fds[CONTROL_POLLFD].events = POLLIN;

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
    event_loop();
    return 0;
}
