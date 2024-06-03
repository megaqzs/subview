#include <pango/pangocairo.h>
#include <unistd.h>
#include "renderer.h"
#include "options.h"
#include "utils.h"

#define FORMAT CAIRO_FORMAT_ARGB32

// get the distance between two consecutive lines in pixel buffer, in bytes
uint32_t get_surf_stride(options_t *options) {
    return cairo_format_stride_for_width(FORMAT, options->width);
}

// draw plain style
static inline void draw_plain(cairo_t *cr, options_t *options, PangoLayout *layout, PangoRectangle *extents, double x, double y) {
    cairo_move_to(cr, x-pango_units_to_double(extents->x), y-pango_units_to_double(extents->y));
    pango_cairo_show_layout(cr, layout);
}

// draw block style
static inline void draw_block(cairo_t *cr, options_t *options, PangoLayout *layout, PangoRectangle *extents, double x, double y) {
    cairo_rectangle(cr,
            x-options->overscan_x,
            y-options->overscan_y,
            pango_units_to_double(extents->width)+2*options->overscan_x,
            pango_units_to_double(extents->height)+2*options->overscan_y);
    cairo_fill(cr);
}

// draw line block style
static inline void draw_line_block(cairo_t *cr, options_t *options, PangoLayout *layout, PangoRectangle *extents, double x, double y) {
    PangoLayoutIter *iter;
    PangoRectangle line_extents;
    iter = pango_layout_get_iter(layout);
    do {
        pango_layout_iter_get_line_extents(iter, NULL, &line_extents);

        cairo_rectangle(cr,
                x-options->overscan_x + pango_units_to_double(line_extents.x),
                y-options->overscan_y + pango_units_to_double(line_extents.y),
                pango_units_to_double(line_extents.width)+2*options->overscan_x,
                pango_units_to_double(line_extents.height)+2*options->overscan_y);
        cairo_fill(cr);
    } while (pango_layout_iter_next_line(iter));
    pango_layout_iter_free(iter);
}

// draw fill style
static inline void draw_fill(cairo_t *cr, options_t *options, PangoLayout *layout, PangoRectangle *extents, double x, double y) {
    cairo_rectangle(cr, 0, 0, options->width, options->height);
    cairo_fill(cr);
}

// const char *text, char *buf, uint32_t stride, options_t *options
void *draw_text(void *arg) {
    PangoLayout *layout;
    PangoFontDescription *desc;
    PangoRectangle extents;
    cairo_t *cr;
    const char *text = ((struct draw_args*) arg)->text;
    char *buf = ((struct draw_args*) arg)->buffer;
    uint32_t stride = ((struct draw_args*) arg)->stride;
    options_t *options = ((struct draw_args*) arg)->options;
    double x,y;

    if (!buf || !options)
        return NULL;
    buf += stride*options->y + options->x;

    cr = cairo_create(cairo_image_surface_create_for_data(buf, FORMAT, options->width, options->height, stride));
    cairo_surface_destroy(cairo_get_target(cr));
    if (!text || *text == 0) {
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_fill(cr);
        goto cairo_end;
    }

    // initialize layout
    desc = pango_font_description_from_string(options->font_desc);
    layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, desc);

    // write the input to the layout (text needs to be constant)
    pango_layout_set_text(layout, text, -1);
    pango_layout_set_width(layout, pango_units_from_double(options->width));

    // get the position for the text
    pango_layout_get_extents(layout, NULL, &extents);
    x = options->width/2 - pango_units_to_double(extents.width)/2; // center
    y = options->height - pango_units_to_double(extents.height); // bottom

    // set the background color and draw it (the things behind the text)
    cairo_set_source_rgba(cr, options->bg.r, options->bg.g, options->bg.b, options->bg.a);
    switch (options->style) {
        case PLAIN:
            break;
        case BLOCK:
            draw_block(cr, options, layout, &extents, x, y);
            break;
        case LINE_BLOCK:
            draw_line_block(cr, options, layout, &extents, x, y);
            break;
        case FILL:
            draw_fill(cr, options, layout, &extents, x, y);
            break;
    }

    // set the foreground color and draw it (the text itself)
    cairo_set_source_rgba(cr, options->fg.r, options->fg.g, options->fg.b, options->fg.a);
    draw_plain(cr, options, layout, &extents, x, y);


    // destroy objects
    g_object_unref(layout);
    pango_font_description_free(desc);
cairo_end:
    cairo_destroy(cr);
    free(arg);
    return NULL;
}
