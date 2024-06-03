#define _STRINGIFY2(x) #x
#define STRINGIFY(x) _STRINGIFY2(x)
#define MESSAGE(stream, lvl, name, M, ...) do { if (log_level <= (lvl)) fprintf(stream, name ": " __FILE__ ":" STRINGIFY(__LINE__) ": " M "\n", ## __VA_ARGS__); } while (0)

#define PDEBUG(M, ...) MESSAGE(stderr, DEBUG, "debug", M, ## __VA_ARGS__)
#define PINFO(M, ...) MESSAGE(stderr, INFO, "info", M, ## __VA_ARGS__)
#define PWARN(M, ...) MESSAGE(stderr, WARN, "warn", M, ## __VA_ARGS__)
#define PERROR(M, ...) MESSAGE(stderr, ERROR, "error", M, ## __VA_ARGS__)
