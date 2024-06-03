#ifndef WAYLAND_WAYLAND_H
#include <wayland-client.h>
#include <pthread.h>
#include "options.h"

//extern struct wl_buffer *buffer;
extern pthread_mutex_t txt_buf_lock;
extern char *inp;
int start_wayland_backend(options_t *);
void update_output(void);
void stop_wayland_backend(void);

#define WAYLAND_WAYLAND_H
#endif
