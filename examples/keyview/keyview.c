#include <libevdev/libevdev.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <libudev.h>
#include <libinput.h>
#include <signal.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

void print_key_event(struct libinput_event *ev);

const char *mapping[] = {
[1] = "Esc",
[15] = "⭾",
[28] = "↵",
[29] = "LCtrl",
[14] = "⌫",
[42] = "⇧", // left shift
[54] = "⇧", // right shift
[56] = "LAlt",
[57] = "␣", // space key
[58] = "⇪", // caps lock
[71] = "⤒", // home
[73] = "⇞", // page up
[79] = "⤓", // end
[81] = "⇟", // page down
[82] = "Ins",
[97] = "RCtrl",
[100] = "RAlt",
[105] = "←",
[106] = "→",
[103] = "↑",
[108] = "↓",
[111] = "⌦", // forward delete key
[125] = "◆", // super/windows key
};

typedef struct list_node {
    struct list_node *next;
    struct list_node *prev;
} list_node_t;

#define CONTAINER_OF(ptr, sample, member)				\
	(__typeof__(sample))((char *)(ptr) -				\
			     offsetof(__typeof__(*sample), member))

#define forward_list_loop(list, var) for ((var) = (list).next; (var) != &(list); (var) = (var)->next)
#define forward_list_loop_safe(list, var, tmp) for ((var) = (list).next, (tmp) = (list).next->next; (var) != &(list); (var) = (tmp), (tmp) = (tmp)->next)
#define LIST_LAST(list) ((list).prev)
#define LIST_FIRST(list) ((list).next)

void remove_node(list_node_t *node) {
    node->next->prev = node->prev;
    node->prev->next = node->next;
}

void insert_node(list_node_t *list, list_node_t *node, int start) {
    if (start) {
        node->prev = list;
        node->next = list->next;
        list->next->prev = node;
        list->next = node;
    } else {
        node->next = list;
        node->prev = list->prev;
        list->prev->next = node;
        list->prev = node;
    }
}

typedef struct {
    const char *key;
    uint32_t keycode;
    list_node_t node;
} key_node_t;

list_node_t keylist = {
    .next = &keylist,
    .prev = &keylist
};

static volatile sig_atomic_t stop = 0;
int sock;

static void exit_hndlr(int signum) {
    write(STDOUT_FILENO, "Interrupt\n", sizeof("Interrupt\n"));
    stop = 1;
}

static const struct sigaction exit_action = {
    .sa_handler = exit_hndlr,
    .sa_flags = SA_RESETHAND
};


static int open_restricted(const char *path, int flags, void *user_data)
{
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}
 
static void close_restricted(int fd, void *user_data)
{
    close(fd);
}
 
const static struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};
 
static int handle_events(struct libinput *li) {
    struct libinput_event *ev;
    libinput_dispatch(li);
    while ((ev = libinput_get_event(li)) != NULL) {
        enum libinput_event_type type = libinput_event_get_type(ev);
        switch (type) {
    		case LIBINPUT_EVENT_DEVICE_ADDED:
                break;
            case LIBINPUT_EVENT_KEYBOARD_KEY:
                print_key_event(ev);
                break;
        }
        libinput_event_destroy(ev);
    }
    fflush(stdout);
}
 
static struct libinput *open_udev(const char *seat, bool verbose, bool *grab)
{
	struct libinput *li;
	struct udev *udev = udev_new();

	if (!udev) {
		fprintf(stderr, "Failed to initialize udev\n");
		return NULL;
	}

	li = libinput_udev_create_context(&interface, grab, udev);
	if (!li) {
		fprintf(stderr, "Failed to initialize context from udev\n");
		goto out;
	}

	//libinput_log_set_handler(li, log_handler);
	if (verbose)
		libinput_log_set_priority(li, LIBINPUT_LOG_PRIORITY_DEBUG);

	if (libinput_udev_assign_seat(li, seat)) {
		fprintf(stderr, "Failed to set seat\n");
		libinput_unref(li);
		li = NULL;
		goto out;
	}

out:
	udev_unref(udev);
	return li;
}

int gen_sock_addr(struct sockaddr_un *addr, size_t pathlen) {
    char *env_path;
    addr->sun_family = AF_UNIX;
    if ((env_path = getenv("SUBVIEW_SOCK")) != NULL) {
        if (strlen(env_path) < pathlen) {
            strcpy(addr->sun_path, env_path);
            return 0;
        }
        errno = ENAMETOOLONG; // environment variable was too long
        return -1;
    } else {
        if (pathlen < snprintf(addr->sun_path, pathlen, "/var/run/user/%d/subview", geteuid())) {
            errno = ENAMETOOLONG; // we failed to create the path with snprintf
            return -1;
        }
        return 0;
    }
}

void print_key_event(struct libinput_event *ev) {
    list_node_t *node, *tmp;
    key_node_t *key, *prev = NULL;
    struct libinput_event_keyboard *kbd_ev = libinput_event_get_keyboard_event(ev);
    uint32_t keycode = libinput_event_keyboard_get_key(kbd_ev);

    if (LIBINPUT_KEY_STATE_PRESSED == libinput_event_keyboard_get_key_state(kbd_ev)) {
        key = malloc(sizeof(key_node_t));

        key->keycode = keycode;
        if (keycode < sizeof(mapping)/sizeof(mapping[0]) && mapping[keycode])
             key->key = mapping[keycode];
        else
             key->key = libevdev_event_code_get_name(EV_KEY, keycode)+sizeof("KEY");

        insert_node(&keylist, &key->node, 0);

        forward_list_loop(keylist, node) {
            key = CONTAINER_OF(node, key, node);
            if (prev) {
                write(sock, prev->key, strlen(prev->key));
                write(sock, "+", 1);
            }
            prev = key;
        }
    } else {
        forward_list_loop_safe(keylist, node, tmp) {
            key = CONTAINER_OF(node, key, node);
            if (key->keycode == keycode) {
                remove_node(node);
                free(key);
                continue;
            }
            if (prev) {
                write(sock, prev->key, strlen(prev->key));
                write(sock, "+", 1);
            }
            prev = key;
        }
    }
    if (prev)
        write(sock, prev->key, strlen(prev->key));
    write(sock, "\0", 1);
}

int main(void) {
    // put the exit handlers as soon as possible
    sigaction(SIGQUIT, &exit_action, NULL);
    sigaction(SIGALRM, &exit_action, NULL);
    sigaction(SIGHUP, &exit_action, NULL);
    sigaction(SIGTERM, &exit_action, NULL);
    sigaction(SIGINT, &exit_action, NULL);

    struct sockaddr_un addr;
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    gen_sock_addr(&addr, sizeof(addr.sun_path));
    connect(sock, (struct sockaddr*) &addr, sizeof(addr));

    struct libinput *li = open_udev("seat0", false, NULL);

    struct pollfd fds = {
	    .fd = libinput_get_fd(li),
	    .events = POLLIN,
	    .revents = 0,
    };

	fds.fd = libinput_get_fd(li);
	fds.events = POLLIN;
	fds.revents = 0;

    if (handle_events(li))
            fprintf(stderr, "Expected device added events on startup but got none. "
                    "Maybe you don't have the right permissions?\n");

	/* time offset starts with our first received event */
	if (poll(&fds, 1, -1) > -1) {
		do {
			handle_events(li);
		} while (!stop && poll(&fds, 1, -1) > -1);
	}

	printf("exiting\n");
    list_node_t *node, *tmp;
    forward_list_loop_safe(keylist, node, tmp) {
        key_node_t *key = CONTAINER_OF(node, key, node);
        free(key); // free the key list on exit
    }
    write(sock, "", 1); // clear the subview buffer so that the text won't stay forever
    close(sock);
    libinput_unref(li);

    return 0;
}
