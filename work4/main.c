#include <logo.h>
#include <request.h>
#include <sharemem.h>

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <subprocess.h>

#include <terminst.h>
#include <time.h>
#include <unistd.h>

#include "input.c"
#include "model.c"

char* progress_bar[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};
const int progress_bar_size = sizeof(progress_bar) / sizeof(progress_bar[0]);
int progress_index = 0;
#define progressing() ({printf("%s\b", progress_bar[progress_index % progress_bar_size]); progress_index++; fflush(stdout);})

struct msg_t {
    int author_id;      // 1 is terminal, 2 is net client for posting msg
    char content[8192];
};
int server_pid;
struct msg_t* shared_msg;
struct msg_t recv_msg;


void server_quit(){
    exit(0);
}

void client_quit(){
    post_signal(server_pid, SIGINT);
    printf("\n");
    exit(0);
}


char* getBody(char* httpResponse) {
    char* body = strstr(httpResponse, "\r\n\r\n");
    if (body) {
        return body + 4;  // 跳过分隔符
    }
    return NULL;  // 未找到正文
}

const char* get_current_time_str() {
    static char time_str[6]; // HH:MM\0
    time_t t = time(NULL);
    struct tm tm_info;
    localtime_r(&t, &tm_info);  // 线程安全版本
    strftime(time_str, sizeof(time_str), "%H:%M", &tm_info);
    return time_str;
}

void stream_print(const char* str){
    char prev = '\n';
    while(*str){
        if(prev == '\n' && *str == '\n'){
            str++;      // 避免连续换行
            continue;
        }
        // # 开头的行，粉色显示
        if(prev == '\n' && *str == '#'){
            printf(COLOR_PINK);
            while(*str && *str != '\n'){
                putchar(*str);
                str++;
            }
            printf(COLOR_RESET "\n");
            prev = *str;
            continue;
        }
        // - 开头的行，把-号变黄
        if(prev == '\n' && *str == '-'){
            printf(COLOR_YELLOW "-" COLOR_RESET);
            prev = *str;
            str++;
            continue;
        }
        putchar(prev = *str);
        fflush(stdout); // 立即刷新输出
        str++;
        usleep(30000);  // 打字机效果，可调
    }
    if (prev != '\n') {
        putchar('\n'); // 回复结束换行
    }
}


fork_func(server){
    signal(SIGINT, server_quit);  // register quit event in server process

    // 初始获取token
    raw_request_t* get_token = raw_init();
    raw_add_line(get_token, "GET /login HTTP/1.1");
    raw_add_line(get_token, "Host: app/login");
    raw_add_line(get_token, "");
    int fd_token = request_create("47.121.28.18", 15444);
    request_send_raw(fd_token, get_token);
    request_recv_all(fd_token, recv_msg.content, 8192);
    request_close(fd_token);
    raw_free(get_token);
    // 解析token
    char* body = getBody(recv_msg.content);
    if(!body){
        body = "Error: Unable to retrieve token from server.\n";
        exit(1);
    }
    char* token = malloc(32);
    sprintf(token, "Auth: %s", body);

    while(true){
        // 获取当前使用的模型
        char current_model[64];
        if (load_current_model(current_model, sizeof(current_model)) == 0) {
            printf(COLOR_YELLOW "No model selected yet.\n" COLOR_RESET);
        }
        // 获取api key
        char key[512];
        if (load_api_key(current_model, key, 512) == 0) {
            printf(COLOR_RED "Error: No API key found for model '%s'.\n" COLOR_RESET, current_model);
            exit(1);
        }
        usleep(100000);  // wait for msg
        smlock(shared_msg);
        if(shared_msg->author_id != 1){  // new msg from terminal
            smunlock(shared_msg);
            continue;
        }

        int  length = strlen(shared_msg->content);
        char content_length[128] = {};
        sprintf(content_length, "Content-Length: %d", length);

        // 拼接成标准的HTTP请求
        raw_request_t* post_chat = raw_init();
        raw_add_line(post_chat, "POST /chat HTTP/1.1");
        raw_add_line(post_chat, "Host: app/chat");
        raw_add_line(post_chat, token);

        // 这里把API Key当作自定义头发送
        char api_key[256];
        sprintf(api_key, "X-API-Key: %s", key);
        raw_add_line(post_chat, api_key);
        // model name
        char model_name[256];
        sprintf(model_name, "Model: %s", current_model);
        raw_add_line(post_chat, model_name);

        raw_add_line(post_chat, "Content-Type: text/plain");
        raw_add_line(post_chat, content_length);
        raw_add_line(post_chat, "");
        raw_add_line(post_chat, shared_msg->content);
        smunlock(shared_msg);

        // 发送请求并获取响应
        int fd = request_create("47.121.28.18", 15444);
        request_send_raw(fd, post_chat);
        request_recv_all(fd, recv_msg.content, 8192);
        request_close(fd);

        // 分析响应体，获取聊天回复
        char* body = getBody(recv_msg.content);
        if(!body){
            body = "Error: Invalid response from server.\n";
        }
        int body_len = strlen(body);
        if(body[body_len - 1] != '\n'){
            body[body_len] = '\n';
            body[body_len + 1] = 0;
        }
        smlock(shared_msg);
        shared_msg->author_id = 2;  // net client replied
        strncpy(shared_msg->content, body, body_len + 2);
        smunlock(shared_msg);
    }
}


bool deal_command(const char* line) {
    switch (match_command(line)) {
        case 0: // /help
            printf(COLOR_YELLOW "Available commands:\n" COLOR_RESET);
            for(int i = 0; commands[i]; i++){
                printf(COLOR_CYAN "  %s\n" COLOR_RESET, commands[i]);
            }
            return true;
        case 1: // /about
            printf(COLOR_YELLOW "Cmini, A Chat Mini CLI Program.\nDeveloped by "
                   COLOR_WHITE "Wu Junkai" COLOR_YELLOW " and "
                   COLOR_WHITE "Liu Huasheng" COLOR_RESET ".\n");
            return true;
        case 2: // /quit
            client_quit();
            return true; // never reach here
        case 3: // /model
            model_selector();
            return true;
        default:
            return false; // not a command
    }
}


int main(){
    // register Ctrl-C signal handler
    signal(SIGINT,  client_quit);
    // register Ctrl-D signal handler
    signal(SIGQUIT, client_quit);

    setup_input();

    // make share memory
    shared_msg = smalloc(10230, sizeof(struct msg_t));
    smlock(shared_msg);
    shared_msg->author_id = 0;  // no msg
    smunlock(shared_msg);

    server_pid = pfork(server);

    // print logo
    printf(COLOR_CYAN logo COLOR_RESET "\n");
    printf(COLOR_GREEN "[%s] <<< "
        COLOR_GRAY "Welcome! use " COLOR_YELLOW "/help" COLOR_GRAY " to get more help.\n" COLOR_RESET,
        get_current_time_str()
    );

    while(true){
        char prompt_buffer[128];
        char *line = NULL;

        // use readline for better input experience
        sprintf(prompt_buffer, COLOR_BLUE "[%s] >>> " COLOR_RESET, get_current_time_str());
        line = readline(prompt_buffer);
        if (deal_command(line)) {
            continue;
        }

        if (line && *line) {
            add_history(line);
        }
        printf(COLOR_GREEN "[%s] <<< " COLOR_RESET, get_current_time_str());

        // copy to shared memory
        smlock(shared_msg);
        shared_msg->author_id = 1;  // terminal input
        strncpy(shared_msg->content, line, sizeof(shared_msg->content));
        smunlock(shared_msg);
        free(line);

        while(true){
            usleep(100000);  // 等待一点时间，避免占用 CPU
            smlock(shared_msg);
            if(shared_msg->author_id != 2){ // not replied yet
                smunlock(shared_msg);
                progressing();
                continue;
            }
            stream_print(shared_msg->content);
            shared_msg->author_id = 0;  // reset
            smunlock(shared_msg);
            break;
        }
    }
}