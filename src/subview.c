#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/un.h>

#include "wayland/wayland.h"
#include "options.h"
#include "utils.h"

int sock = -1;
struct sockaddr_un addr = {0};

// PINFO won't work acording to signal-safety(7) man page
void exit_hndlr(int signum) {
    write(STDERR_FILENO, "Interrupted\n", sizeof("Interrupted\n"));
    closed = true;
}

int gen_sock_addr(struct sockaddr_un *addr, size_t pathlen, options_t *options) {
    addr->sun_family = AF_UNIX;
    size_t path_size = 0;
    if (options->path) {
        path_size = strlen(options->path);
        if (pathlen < path_size) {
            errno = ENAMETOOLONG;
            return -1;
        }
        memcpy(addr->sun_path, options->path, path_size);
    } else {
        path_size = snprintf(addr->sun_path, pathlen, "/var/run/user/%d/subview", geteuid());;
        if (pathlen < path_size) {
            errno = ENAMETOOLONG;
            return -1;
        }
    }
    if (options->force)
        unlink(addr->sun_path);
    return 0;
}

int main(int argc, char *argv[]) {
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
        if (gen_sock_addr(&addr, sizeof(addr.sun_path), options) || bind(sock, (struct sockaddr*) &addr, sizeof(addr))) {
            switch (errno) {
                case ENAMETOOLONG:
                    PERROR("Failed to create control socket because it's file name is too long");
                    break;
                case EADDRINUSE:
                    PERROR("Failed to create control socket at '%s' because it is already in use, if there is no other instance of subview running use -f to ignore it", addr.sun_path);
                    break;
                default:
                    PERROR("Failed to create control socket at '%s' with error `%s`", addr.sun_path, strerror(errno));
                    break;
            }
            close(sock);
            sock = -1; // socket failed to connect so can't unlink since it might belong to another process
            exit_code = -1;
            goto end;
        }
        // put the exit handlers as soon as possible
        sigaction(SIGQUIT, &exit_action, NULL);
        sigaction(SIGALRM, &exit_action, NULL);
        sigaction(SIGHUP, &exit_action, NULL);
        sigaction(SIGTERM, &exit_action, NULL);
        sigaction(SIGINT, &exit_action, NULL);

        if (options->print_path)
            puts(addr.sun_path); // print the socket path if requested, must be before deamonisation

        if (options->daemonise)
            daemon(1, 1); // daemonise after socket creation if requested

        listen(sock, 5);
        start_wayland_backend(options, sock);
    }
end:
    PDEBUG("exiting safely");
    if (sock >= 0) {
        close(sock);
        unlink(addr.sun_path);
    }
    free(options);
    for (int i = 0; i < MAX_LOG_FILES; i++) {
        fclose(log_files[i]);
        log_files[i] = NULL;
    }
    exit(exit_code);
}
