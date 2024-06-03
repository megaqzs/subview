#include <stdint.h>
#include <stdbool.h>
#include "zwlr-shell.h"

#ifndef OPTIONS_H
enum text_style {
    PLAIN, // only displays the text without any background
    BLOCK, // displays the text in front of a box for the entire input
    LINE_BLOCK, // displays the text in front of a box for every line
    BORDER, // adds a border around the text based on it's shape
    FILL // fills the whole buffer with background, for debuging
};

enum log_level {
    DEBUG,
    VERBOSE,
    INFO,
    WARN,
    ERROR
};

struct color {
    double r;
    double g;
    double b;
    double a;
};

typedef struct {
    enum log_level level;
    enum text_style style;
    enum zwlr_layer_surface_v1_anchor anchor;
    struct color bg;
    struct color fg;
    uint32_t width; // the maximal width of subtitles
    uint32_t height; // the maximal height of subtitles
    uint32_t margin_top;
    uint32_t margin_bottom;
    uint32_t margin_right;
    uint32_t margin_left;
    char *font_desc;
    bool align_center; 
    
    // only applies for border style
    uint32_t border_strength;

    // only applies for paragraph_block and line_block styles
    uint32_t overscan_x;
    uint32_t overscan_y;
} options_t;

int parse_args(int argc, char *argv[], options_t *);
void get_help(char *);
#   define OPTIONS_H
#endif
