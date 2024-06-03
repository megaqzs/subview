#ifndef WAYLAND_WAYLAND_H
#include <wayland-client.h>
#include "options.h"

//extern struct wl_buffer *buffer;
int start_wayland_backend(options_t *, int);
extern _Atomic bool closed;

#define WAYLAND_WAYLAND_H
#endif
