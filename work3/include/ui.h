#ifndef __UI_H__
#define __UI_H__

#include <terminst.h>
#include <stdbool.h>

void ui_square(widget_t* widget, int col, int lin, 
    int height, int width, color_t color);

void ui_text(widget_t* widget, int col, int lin, 
    const char* text, color_t color);

void ui_button(widget_t* widget, int col, int lin, int width,
    const char* text, color_t color, bool selected, int align);

#endif//__UI_H__