#ifndef CONNECTION_H
#define CONNECTION_H

#include <poll.h>
#include <unistd.h>
#include "list.h"

extern struct list_head connection_list;
#define INITIAL_BUFFER_SIZE 3
typedef struct {
    size_t vis_size;
    char *vis_buff; // the buffer that is currently used for rendering

    int inp_pos; // the position of the next read input data in the buffer
    size_t inp_size;
    char *inp_buff; // the buffer for input data from file descriptor

    int updcount; // the number of times the data has beed updated, usefull for detecting changes to vis_buff
    int refcount; // reference count for the connection
    struct list_head link;
    struct pollfd *pfd;
} connection_t;

connection_t *new_connection(struct pollfd *pfd, int fd);

void reftick_connection(connection_t *connection);
void refdrop_connection(connection_t *conn);

int update_connection(connection_t *conn);

#endif
