#include <stdbool.h>
#include <terminst.h>
#include <ui.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void print_line(const char* start, const char* middle, int repeat, const char* end){
    printf(start);
    for(int i = 0; i < repeat; i++){
        printf(middle);
    }
    printf(end);
}

void ui_square(widget_t* widget, int col, int lin, int height, int width, color_t color){
    if(!widget){
        return;
    }
    int abs_x = widget->x_start + widget->has_border + col;
    int abs_y = widget->y_start + widget->has_border + lin;
    printf(color);
    if(height == 1){
        terminst_move(abs_x, abs_y);
        print_line("[", "-", width - 2, "]");
        printf(COLOR_RESET);
        return;
    }
    terminst_move(abs_x, abs_y);
    print_line("┌", "─", width - 2, "┐");
    for(int j = 1; j < height - 1; j++){
        terminst_move(abs_x, abs_y + j);
        print_line("│", " ", width - 2, "│");
    }
    terminst_move(abs_x, abs_y + height - 1);
    print_line("└", "─", width - 2, "┘");
    printf(COLOR_RESET);
}

void ui_text(widget_t* widget, int col, int lin, const char* text, color_t color){
    if(!widget || !text){
        return;
    }
    int abs_x = widget->x_start + widget->has_border + col;
    int abs_y = widget->y_start + widget->has_border + lin;
    int str_len = strlen(text);
    char* copy = malloc(str_len + 1);
    strncpy(copy, text, str_len);
    char* line = strtok(copy, "\n");
    printf(color);
    while(line){
        terminst_move(abs_x, abs_y);
        printf("%s", line);
        line = strtok(NULL, "\n");
        abs_y++;
    }
    printf(COLOR_RESET);
    free(copy);
}

void ui_button(widget_t* widget, int col, int lin, int width, const char* text, color_t color, bool selected, int align){
    if(!widget || !text){
        return;
    }
    int abs_x = widget->x_start + widget->has_border + col;
    int abs_y = widget->y_start + widget->has_border + lin;
    // first print the button border
    if(selected){
        printf(color);
    } else {
        printf(COLOR_WHITE);
    }
    ui_square(widget, col, lin, 3, width, selected ? color : COLOR_WHITE);
    int str_len = strlen(text);
    if(align == 0){ // 左对齐
        // do nothing
    }else if(align == 1){ // 居中
        abs_x += (width - str_len) / 2;
    }else if(align == 2){ // 右对齐
        abs_x += width - 1 - str_len;
    }
    terminst_move(abs_x, abs_y + 1);
    printf(text);
    printf(COLOR_RESET);
}