#ifndef __TERMINST_H__
#define __TERMINST_H__

typedef const char* color_t;
#define COLOR_BLACK   "\033[30m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_GRAY    "\033[90m"
#define COLOR_RESET   "\033[0m"
#define COLOR_ORANGE  "\033[38;5;208m"  // 橙色
#define COLOR_PINK    "\033[38;5;205m"  // 粉色
#define COLOR_PURPLE  "\033[38;5;129m"  // 紫色
#define COLOR_BROWN   "\033[38;5;130m"  // 棕色
#define COLOR_GOLD    "\033[38;5;220m"  // 金色
#define COLOR_SILVER  "\033[38;5;250m"  // 银色
#define COLOR_LIME    "\033[38;5;154m"  // 柠檬绿
#define COLOR_NAVY    "\033[38;5;17m"   // 海军蓝
#define COLOR_TEAL    "\033[38;5;30m"   // 青绿色
#define COLOR_OLIVE   "\033[38;5;142m"  // 橄榄绿
#define COLOR_MAROON  "\033[38;5;88m"   // 栗色
#define COLOR_INDIGO  "\033[38;5;54m"   // 靛蓝

typedef enum {
    KEY_NONE = 0,
    KEY_UP = 1000,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_ENTER = '\n',
    KEY_ESC = 27,
    KEY_SPACE = ' ',
    KEY_BACKSPACE = 127,
    KEY_TAB = '\t',
} key_code_t;

typedef struct{
    int x_start;
    int y_start;
    int width;
    int height;
    int has_border;
} widget_t;

void terminst_init();
void terminst_quit();
void terminst_exit(int code);
void terminst_throw(const char* msg);
void terminst_safe_exit();
void terminst_clear();
void terminst_size(int* rows, int* cols);
void terminst_flush();
void terminst_move(int x, int y);
void terminst_echo(int show);

key_code_t terminst_get_key();
key_code_t terminst_get_game_key();
key_code_t terminst_wait_key();
int        terminst_key_available();
void       terminst_key_flush();

widget_t* widget_create(int x_start, int y_start, int width, int height);
void      widget_free(widget_t* widget);
void      widget_border(widget_t* widget);
void      widget_clear(const widget_t* widget);
void      widget_print(const widget_t* widget, int x, int y, const char* fmt, color_t);

#endif//__TERMINST_H__