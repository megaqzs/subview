#ifndef WAYLAND_WAYLAND_H
#include <wayland-client.h>
#include <pthread.h>

//extern struct wl_buffer *buffer;
extern pthread_mutex_t txt_buf_lock;
extern char inp[40];
void *start_wayland_backend(void *);
void update_output(void);
void stop_wayland_backend(void);

#define WAYLAND_WAYLAND_H
#endif
