#include <pango/pangocairo.h>
#include "options.h"

#define FORMAT CAIRO_FORMAT_ARGB32

uint32_t get_surf_stride(options_t *options) {
    return options->height*cairo_format_stride_for_width(FORMAT, options->width);
}

void draw_text(char *text, char *buf, uint32_t stride, options_t *options) {
    cairo_surface_t *cr_surf;
    cairo_t *cr;
    PangoLayout *layout;
    PangoFontDescription *desc;
    PangoRectangle extents;
    double x,y;

    cr_surf = cairo_image_surface_create_for_data(buf, FORMAT, options->width, options->height, stride);
    cr = cairo_create(cr_surf);
    cairo_surface_destroy(cr_surf);

    // initialize layout
    desc = pango_font_description_from_string(options->font_desc);
    layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, desc);

    // write the input to the layout without the input changing
    pango_layout_set_text(layout, text, -1);
    pango_layout_set_width(layout, pango_units_from_double(options->width));


    // get text positioning information
    pango_layout_get_extents(layout, NULL, &extents);
    x = options->width/2 - pango_units_to_double(extents.width/2);
    y = options->height - pango_units_to_double(extents.height);

    // draw background behind text
    cairo_set_source_rgba(cr, options->bg.r, options->bg.g, options->bg.b, options->bg.a);
    cairo_rectangle(cr,
            x-options->overscan_x,
            y-options->overscan_y,
            pango_units_to_double(extents.width)+2*options->overscan_x,
            pango_units_to_double(extents.height)+2*options->overscan_y);
    cairo_fill(cr);

    // draw text
    cairo_move_to(cr, x-pango_units_to_double(extents.x), y-pango_units_to_double(extents.y));
    cairo_set_source_rgba(cr, options->fg.r, options->fg.g, options->fg.b, options->fg.a);
    pango_cairo_show_layout(cr, layout);

    cairo_destroy(cr);
    pango_font_description_free(desc);
}
