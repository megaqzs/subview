#include <pango/pangocairo.h>
#include "options.h"

#define FORMAT CAIRO_FORMAT_ARGB32

uint32_t get_surf_stride(options_t *options) {
    return options->height*cairo_format_stride_for_width(FORMAT, options->width);
}

static inline void draw_plain(cairo_t *cr, options_t *options, PangoLayout *layout, PangoRectangle *extents, double x, double y) {
    cairo_move_to(cr, x-pango_units_to_double(extents->x), y-pango_units_to_double(extents->y));
    pango_cairo_show_layout(cr, layout);
}

static inline void draw_block(cairo_t *cr, options_t *options, PangoLayout *layout, PangoRectangle *extents, double x, double y) {
    cairo_rectangle(cr,
            x-options->overscan_x,
            y-options->overscan_y,
            pango_units_to_double(extents->width)+2*options->overscan_x,
            pango_units_to_double(extents->height)+2*options->overscan_y);
    cairo_fill(cr);
}

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

static inline void draw_fill(cairo_t *cr, options_t *options, PangoLayout *layout, PangoRectangle *extents, double x, double y) {
    cairo_rectangle(cr, 0, 0, options->width, options->height);
    cairo_fill(cr);
}

void draw_text(char *text, char *buf, uint32_t stride, options_t *options) {
    cairo_surface_t *cr_surf;
    cairo_t *cr;
    PangoLayout *layout;
    PangoFontDescription *desc;
    double x,y;

    cr_surf = cairo_image_surface_create_for_data(buf, FORMAT, options->width, options->height, stride);
    cr = cairo_create(cr_surf);
    cairo_surface_destroy(cr_surf);

    // initialize layout
    desc = pango_font_description_from_string(options->font_desc);
    layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, desc);

    // write the input to the layout (text needs to be constant)
    pango_layout_set_text(layout, text, -1);
    pango_layout_set_width(layout, pango_units_from_double(options->width));

    // draw text
    PangoRectangle extents;
    pango_layout_get_extents(layout, NULL, &extents);
    x = options->width/2 - pango_units_to_double(extents.width/2);
    y = options->height - pango_units_to_double(extents.height);

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
    cairo_set_source_rgba(cr, options->fg.r, options->fg.g, options->fg.b, options->fg.a);
    draw_plain(cr, options, layout, &extents, x, y);


    g_object_unref(layout);
    cairo_destroy(cr);
    pango_font_description_free(desc);
}
