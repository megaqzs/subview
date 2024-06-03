#include "options.h"

#define PROGNAME "subview"
#define VERSION "0.1-devel"

// the default options used by the program
options_t default_options = {
    .level = WARN,
    .style = BLOCK,
    .anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
    .fg = {
        .r = 1,
        .g = 0,
        .b = 0,
        .a = 1
    },
    .bg = {
        .r = .2,
        .g = .2,
        .b = .2,
        .a = .7
    },
    .width = 1500,
    .height = 500,
    .font_desc = "Sans 52",
    .border_strength = 100,
    .align_center = false,

    .margin_top = 0,
    .margin_bottom = 100,
    .margin_right = 0,
    .margin_left = 0,
    .overscan_x = 10,
    .overscan_y = 0,
};