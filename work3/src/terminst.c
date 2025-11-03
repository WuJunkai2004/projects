#include "terminst.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>

static struct termios oldt;

void terminst_init(){
    tcgetattr(STDIN_FILENO, &oldt);
    printf("\033[?1049h"); // 切换到备用缓冲区
    printf("\033[2J");     // 清屏
    printf("\033[H");      // 光标移动到左上角
    printf("\033[?25l");   // 隐藏光标
    struct termios newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // 关闭缓冲和回显
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    fflush(stdout);
}

void terminst_quit(){
    printf("\033[?1049l"); // 退出备用缓冲区
    printf("\033[?25h");   // 显示光标
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fflush(stdout);
}

void terminst_exit(int code){
    terminst_quit();
    exit(code);
}

void terminst_throw(const char* msg){
    terminst_quit();
    if(msg){
        fprintf(stderr, "Error: %s\n", msg);
    } else {
        fprintf(stderr, "An error occurred.\n");
    }
    exit(1);
}

void terminst_safe_exit(){
    terminst_exit(0);
}

void terminst_clear(){
    printf("\033[2J"); // 清屏
    printf("\033[H");  // 移动光标到左上角
    fflush(stdout);
}

void terminst_size(int* rows, int* cols){
    struct winsize w;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &w);
    *rows = w.ws_row;
    *cols = w.ws_col;
}

void terminst_flush(){
    fflush(stdout);
}

void terminst_echo(int show){
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    if(show){
        term.c_lflag |= ECHO;
    } else {
        term.c_lflag &= ~ECHO;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void terminst_move(int x, int y){
    printf("\033[%d;%dH", y + 1, x + 1); // ANSI转义序列，移动光标
}

int terminst_key_available(void) {
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

void terminst_key_flush(){
    tcflush(STDIN_FILENO, TCIFLUSH);
}

inline static key_code_t __get_key(){
    char chr;
    if(read(STDIN_FILENO, &chr, 1) != 1){
        return KEY_NONE;
    }
    if(chr == '\033'){
        if(!terminst_key_available()){
            return KEY_ESC;
        }
        char seq[3];
        if(read(STDIN_FILENO, &seq[0], 1) != 1){
            return KEY_ESC;
        }
        if(read(STDIN_FILENO, &seq[1], 1) != 1){
            return KEY_ESC;
        }
        if(seq[0] == '['){
            switch(seq[1]){
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
                default: return KEY_NONE;
            }
        }
        return KEY_ESC;
    }
    switch(chr){
        case '\n':
        case '\r': return KEY_ENTER;
        case 127:
        case '\b': return KEY_BACKSPACE;
        case '\t': return KEY_TAB;
        case ' ': return KEY_SPACE;
        default:
            if(chr >= 'a' && chr <= 'z'){
                chr ^= ' '; // 转大写
            }
            return (key_code_t)chr;
    }
}

key_code_t terminst_get_key(){
    if(!terminst_key_available()){
        return KEY_NONE;
    }
    return __get_key();
}

key_code_t terminst_get_game_key(){
    key_code_t latest_key = KEY_NONE;
    while(terminst_key_available()){
        latest_key = __get_key();
    }
    return latest_key;
}

key_code_t terminst_wait_key(){
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    while(select(STDIN_FILENO + 1, &fds, NULL, NULL, NULL) < 1);
    return __get_key();
}

widget_t* widget_create(int x_start, int y_start, int width, int height){
    widget_t* widget = (widget_t*)malloc(sizeof(widget_t));
    if(widget){
        widget->x_start = x_start;
        widget->y_start = y_start;
        widget->width = width;
        widget->height = height;
    }
    return widget;
}

void widget_free(widget_t* widget){
    free(widget);
}

void widget_border(widget_t* widget){
    if(!widget){
        return;
    }
    widget->has_border = 1;
    int x = widget->x_start;
    int y = widget->y_start;
    int w = widget->width;
    int h = widget->height;
    // 绘制顶部边框
    terminst_move(x, y);
    printf("+");
    for(int i = 0; i < w - 2; i++){
        printf("-");
    }
    printf("+");
    // 绘制侧边框
    for(int j = 1; j < h - 1; j++){
        terminst_move(x, y + j);
        printf("|");
        terminst_move(x + w - 1, y + j);
        printf("|");
    }
    // 绘制底部边框
    terminst_move(x, y + h - 1);
    printf("+");
    for(int i = 0; i < w - 2; i++){
        printf("-");
    }
    printf("+");
}

void widget_clear(const widget_t* widget){
    if(!widget) return;
    for(int j = 1; j < widget->height - 1; j++){
        terminst_move(widget->x_start + 1, widget->y_start + j);
        for(int i = 1; i < widget->width - 1; i++){
            printf(" ");
        }
    }
}

void widget_print(const widget_t* widget, int x, int y, const char* fmt, color_t color){
    if(!widget || !fmt) return;
    int abs_x = widget->x_start + 1 + x;
    int abs_y = widget->y_start + 1 + y;
    if(abs_x < widget->x_start + 1 || abs_x >= widget->x_start + widget->width - 1 ||
       abs_y < widget->y_start + 1 || abs_y >= widget->y_start + widget->height - 1){
        return; // 超出边界
    }
    terminst_move(abs_x, abs_y);
    if(color){
        printf("%s", color);
    }
    printf(fmt);
    if(color){
        printf(COLOR_RESET);
    }
}
