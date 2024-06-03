#include <stdint.h>
#include "options.h"

#ifndef PANGOCAIRO_RENDERER_H
    uint32_t get_surf_stride(options_t *);
    void draw_text(const char *, char *, uint32_t, options_t *);
#   define PANGOCAIRO_RENDERER_H
#endif
