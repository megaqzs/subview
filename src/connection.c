#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include "connection.h"
#include "utils.h"

// can only be used with variables and constants
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

connection_t *new_connection(struct pollfd *pfd, int fd) {
    connection_t *conn = malloc(sizeof(connection_t));
    if (conn == NULL)
        return NULL;
    conn->pfd = pfd;

    // set the fd to non blocking mode for reading data
    int flags = fcntl(fd, F_GETFL, 0) | O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);

    pfd->fd = fd;
    pfd->events = POLLIN; // we only need to read data from clients

    conn->updcount = 0; // update counter for new data detection
    conn->inp_pos = conn->inp_size = conn->vis_size = 0;
    conn->inp_buff = conn->vis_buff = NULL;
    conn->refcount = 1; // set to 1 because the caller has a reference
    return conn;
}

void reftick_connection(connection_t *connection) {
    connection->refcount++;
}
void refdrop_connection(connection_t *conn) {
    if (!(--conn->refcount)) {
        PDEBUG("freeing connection for fd %d", conn->pfd->fd);
        close(conn->pfd->fd);
        conn->pfd->fd = -conn->pfd->fd;
        free(conn->vis_buff);
        free(conn->inp_buff);
        free(conn);
    }
}

static ssize_t extend_buffer_connection(char **buff, size_t *size, size_t min_size) {
    char *ret;
    size_t new_size = *size;
    if ((new_size *= 2) < min_size)
        new_size = min_size;
    if (*size >= new_size)
        return *size;

    if (!(ret = realloc(*buff, new_size)))
        return -1;
    *buff = ret;
    *size = new_size;
    return new_size;
}

static int finish_message_connection(connection_t *conn) {
    char *end;
    while (conn->inp_pos && (end = memchr(conn->inp_buff, 0, conn->inp_pos))) {
        conn->updcount++; // used for detecting frame updates
        conn->inp_pos -= end - conn->inp_buff + 1;

        // swap vis_buff with inp_buff
        size_t tmp = conn->vis_size;
        char *tmp_ptr = conn->vis_buff;
        conn->vis_buff = conn->inp_buff;
        conn->inp_buff = tmp_ptr;
        conn->vis_size = conn->inp_size;
        conn->inp_size = tmp;

        if (conn->inp_size < conn->inp_pos && extend_buffer_connection(&conn->inp_buff, &conn->inp_size, MAX(INITIAL_BUFFER_SIZE, conn->inp_pos)) == -1)
            return -1;
        memcpy(conn->inp_buff, end+1, conn->inp_pos);
    }
    return 0;
}

int update_connection(connection_t *conn) {
    ssize_t count = 0;
    struct pollfd *pfd = conn->pfd;
    if (pfd->fd < 0 || pfd->revents & (POLLERR | POLLHUP | POLLNVAL))
        return -1;
    if (pfd->revents == 0)
        return 0;
    pfd->revents = 0; // event is handled here, so no need to update again for different reference update call

    do {
        if (conn->inp_buff && conn->inp_size > conn->inp_pos)
            count = read(pfd->fd, conn->inp_buff + conn->inp_pos, conn->inp_size - conn->inp_pos);
        else
            count = 0;
        conn->inp_pos += MAX(count, 0);
        if (count < 0 || count > 0 && finish_message_connection(conn) < 0)
            return -1;
        if (conn->inp_pos == conn->inp_size) {
            if (0 > extend_buffer_connection(&conn->inp_buff, &conn->inp_size, INITIAL_BUFFER_SIZE))
                return -1;
            continue;
        }
    } while (false);
    return 0;
}
