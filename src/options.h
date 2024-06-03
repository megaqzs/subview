#include <stdint.h>
#include <stdbool.h>
#include "zwlr-shell.h"

#ifndef OPTIONS_H
enum text_style {
    PLAIN, // only displays the text without any background
    BLOCK, // displays the text in front of a box for the entire input
    LINE_BLOCK, // displays the text in front of a box for every line
    FILL // fills the whole buffer with background, for debuging
};

enum verbosity {
    DEBUG,
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
    
    // only applies for paragraph_block and line_block styles
    int32_t overscan_x;
    int32_t overscan_y;
} options_t;

int parse_args(int argc, char *argv[], options_t *);
void get_help(char *);
extern enum verbosity log_level;
#   define OPTIONS_H
#endif
