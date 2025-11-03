#include <stddef.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <terminst.h>

char* commands[] = {
    "/help",
    "/about",
    "/quit",
    "/model",
    NULL
};


char* command_generator(const char* text, int state) {
    static int list_index, len;
    char* name;

    if (!state) {
        list_index = 0;
        len = strlen(text);
    }

    while ((name = commands[list_index++])) {
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }

    return NULL;
}

char** command_completion(const char* text, int start, int end) {
    rl_attempted_completion_over = 1;
    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }
    return NULL;
}


int suggestion_hook() {
    char suggestion[128] = {0};

    // 如果当前没有行缓冲或不是以 '/' 开头，则不提供命令建议
    if (!rl_line_buffer || rl_line_buffer[0] != '/') {
        return 0; // 不显示建议，继续运行
    }

    int len = strlen(rl_line_buffer);
    for(int i=0; commands[i]; i++){
        const char* name = commands[i];
        // 如果命令与当前输入前缀匹配
        if (strncmp(name, rl_line_buffer, len) == 0) {
            strncpy(suggestion, name, sizeof(suggestion) - 1);
            break;
        }
    }

    // 只有在存在唯一建议且建议与输入不完全相同时才显示
    if (strcmp(suggestion, rl_line_buffer) == 0) {
        return 0;
    }

    int suggestion_len = strlen(suggestion);
    if (suggestion_len < len) {
        return 0; // 建议比输入短，不显示建议
    }
    
    printf(COLOR_GRAY);
    for(int i=len; i<suggestion_len; i++){
        putchar(suggestion[i]);
    }
    printf(COLOR_RESET);
    for(int i=len; i<suggestion_len; i++){
        putchar('\b');
    }
    fflush(stdout);

    return 0;
}


void setup_input(){
    rl_attempted_completion_function = command_completion;
    rl_event_hook = suggestion_hook;
}


int match_command(const char* line){
    for(int i = 0; commands[i]; i++){
        if (strncmp(line, commands[i], strlen(commands[i])) == 0) {
            return i;
        }
    }
    return -1;
}

// if need to continue the program, return true; else false
bool command_input(const char* line, void (*quit_func)(void)) {
    if (strncmp(line, "/quit", 5) == 0) {
        quit_func();
        return true;
    }
    if (strncmp(line, "/help", 5) == 0) {
        printf(COLOR_YELLOW "Commands: /quit, /help\n" COLOR_RESET);
        return true;
    }
    return false;
}