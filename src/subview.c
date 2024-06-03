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
    if (signum >= 0) // if it is a real signal
        write(STDOUT_FILENO, "Interrupted\n", sizeof("Interrupted\n")); 

    if (sock >= 0) {
        close(sock);
        unlink(addr.sun_path);
    }
    closed = true;
}

int gen_sock_addr(struct sockaddr_un *addr, size_t pathlen) {
    addr->sun_family = AF_UNIX;
    if (pathlen < snprintf(addr->sun_path, pathlen, "/var/run/user/%d/subview", geteuid())) {
        errno = ENAMETOOLONG; // we failed to create the path with snprintf
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    bool running = true;
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
        if (gen_sock_addr(&addr, sizeof(addr.sun_path)) || bind(sock, (struct sockaddr*) &addr, sizeof(addr))) {
            if (errno == ENAMETOOLONG)
                PERROR("Failed to create control socket since path was too long, exiting due to unexpected behavior", strerror(errno));
            else
                PERROR("Failed to create control socket at '%s' with error `%s`, exiting due to unexpected behavior", addr.sun_path, strerror(errno));
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

        listen(sock, 0); // no need for backlog since it is a single client program
        start_wayland_backend(options, sock);
    }
end:
    PDEBUG("exiting safely");
    free(options);
    for (int i = 0; i < MAX_LOG_FILES; i++) {
        fclose(log_files[i]);
        log_files[i] = NULL;
    }
    exit_hndlr(-1); // no signal number
    exit(exit_code);
}
