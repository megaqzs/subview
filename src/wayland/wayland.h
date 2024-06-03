#ifndef WAYLAND_WAYLAND_H
#include <wayland-client.h>

//extern struct wl_buffer *buffer;
extern uint32_t inp;
void *start_wayland_backend(void *);
void update_output(void);
void stop_wayland_backend(void);

#define WAYLAND_WAYLAND_H
#endif
