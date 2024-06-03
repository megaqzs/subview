#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/un.h>

#include "wayland/wayland.h"
#include "options.h"
#include "utils.h"

// platform dependent, 108 is used in order to be safe
#define UNSOCKET_PATH_LENGTH 108

int sock = -1;
struct sockaddr_un addr = {0};

// PINFO won't work acording to signal-safety(7) man page
void exit_hndlr(int signum) {
    if (signum >= 0) // if it is a real signal
        write(STDOUT_FILENO, "Interrupted\n", sizeof("Interrupted\n")); 

    if (sock >= 0) {
        close(sock);
        unlink(addr.sun_path);
    }
    if (signum >= 0) {
        // let the operating system handle resource destruction
        // to avoid unnecessary design requirments which could cause problems
        _exit(0);
    }
}

int bind_unix_sock(int sock, struct sockaddr_un *addr, socklen_t addrlen) {
    errno = ENAMETOOLONG; // set it in case we fail to create the path prefix with snprintf
    addr->sun_family = AF_UNIX;
    if (UNSOCKET_PATH_LENGTH < snprintf(addr->sun_path, UNSOCKET_PATH_LENGTH, "/var/run/user/%d/subview", geteuid()) ||
        bind(sock, (struct sockaddr*) addr, addrlen))
        return -1;
    return 0;
}

int main(int argc, char*argv[]) {
    int exit_code = 0;
    struct sigaction exit_action = {
        .sa_handler = exit_hndlr,
        .sa_flags = SA_RESETHAND
    };

    ssize_t ilen;
    size_t len = 0;
    char *line = NULL;
    options_t *options = malloc(sizeof(*options));
    if (!parse_args(argc, argv, options)) {

        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (bind_unix_sock(sock, &addr, sizeof(addr))) {
            if (errno == ENAMETOOLONG)
                PERROR("Failed to create control socket since path was too long, exiting due to unexpected behavior", strerror(errno));
            else
                PERROR("Failed to create control socket at '%s' with error `%s`, exiting due to unexpected behavior", addr.sun_path, strerror(errno));
            close(sock);
            sock = -1; // socket failed to connect so can't unlink
            exit_code = -1;
            goto end;
        }
        if (options->daemonise)
            daemon(0, 0); // daemonise after socket creation

        // put the exit handlers as soon as possible
        sigaction(SIGQUIT, &exit_action, NULL);
        sigaction(SIGINT, &exit_action, NULL);

        if (options->print_path)
            puts(addr.sun_path); // print the socket path if requested
        listen(sock, 0);
        FILE *conn = fdopen(accept(sock, NULL, NULL), "r+b");

        if (!start_wayland_backend(options)) {
            while ((ilen = getline(&line, &len, conn)) > 0) {
                if (ilen > 0 && line[ilen-1] == '\n') {
                    line[--ilen] = '\0';
                    if (ilen > 0 && line[ilen-1] == '\r')
                        line[--ilen] = '\0';
                }
                PDEBUG("line length is %ld", ilen);
                if (!ilen)
                    break;

                pthread_mutex_lock(&txt_buf_lock);
                free(inp);
                inp = malloc(ilen+1);
                memcpy(inp, line, ilen+1);
                update_output();
                pthread_mutex_unlock(&txt_buf_lock);
            }
        }
    }
end:
    stop_wayland_backend();
    free(line);
    free(options);
    free(inp);
    exit_hndlr(-1); // no signal number
    exit(exit_code);
}
