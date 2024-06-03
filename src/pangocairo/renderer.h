#include <stdint.h>
#include "options.h"

#ifndef PANGOCAIRO_RENDERER_H
#   define PANGOCAIRO_RENDERER_H
    struct draw_args {
        const char *text;
        char *buffer;
        uint32_t stride;
        options_t *options;
    };

    uint32_t get_surf_stride(options_t *);
    // const char *, char *, uint32_t, options_t *
    void *draw_text(void *);
#endif
