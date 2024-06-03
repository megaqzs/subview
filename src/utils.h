#define MESSAGE_NAME_PREFIX "subview: "

#define _STRINGIFY2(x) #x
#define STRINGIFY(x) _STRINGIFY2(x)

#define MESSAGE(lvl, name, M, ...) _logger(lvl, MESSAGE_NAME_PREFIX name ": " __FILE__ ":" STRINGIFY(__LINE__) ": " M "\n", ## __VA_ARGS__)

#define PDEBUG(M, ...) MESSAGE(DEBUG, "debug", M, ## __VA_ARGS__)
#define PINFO(M, ...) MESSAGE(INFO, "info", M, ## __VA_ARGS__)
#define PWARN(M, ...) MESSAGE(WARN, "warn", M, ## __VA_ARGS__)
#define PERROR(M, ...) MESSAGE(ERROR, "error", M, ## __VA_ARGS__)
