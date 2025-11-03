#include "server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <dirent.h>
#include <c_types.h>

typedef enum {
    HTTP_STATE_REQUEST_LINE,    // 解析请求行: GET / HTTP/1.1
    HTTP_STATE_HEADERS,         // 解析头部: Host: example.com
    HTTP_STATE_BODY,            // 解析body数据
    HTTP_STATE_COMPLETE         // 请求解析完成
} http_parse_state_t;

typedef struct {
    http_parse_state_t state;
    dict headers;
    const char* request_line;  // 存储请求行
    const char* method;        // 存储请求方法，如GET、POST
    const char* url_path;      // 存储请求的URL路径
    int content_length;        // 从Content-Length头获取
    int current_length;        // 当前已接收的body长度
    char body[0];              // 动态分配的body数据，使用flexible array member
} http_context_t;

// 为每个客户端绑定HTTP上下文
void http_handle_complete_request(server_t* server, client_context_t* client, http_context_t* ctx);

int fd_mapping_http[MAX_CLIENTS] = {0}; // 简单的文件描述符映射
// 辅助函数：根据fd获取HTTP上下文索引
static int  client_get_index(int fd);
static int  client_new_index(int fd);
static void client_del_index(int fd);

// 根据文件描述符查找HTTP上下文索引
static int client_get_index(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (fd_mapping_http[i] == fd) {
            return i;
        }
    }
    return -1; // 未找到
}

// 为文件描述符分配新的HTTP上下文索引
static int client_new_index(int fd) {
    int idx = client_get_index(0);
    if (idx != -1) {
        fd_mapping_http[idx] = fd;
    }
    return idx; // 返回索引或-1表示失败
}

// 释放文件描述符的HTTP上下文索引
static void client_del_index(int fd) {
    int idx = client_get_index(fd);
    if (idx != -1) {
        fd_mapping_http[idx] = 0;
    }
}


static http_context_t* client_http_contexts[MAX_CLIENTS] = {0};

void http_init_client(server_t* server, client_context_t* client) {
    // 为新客户端创建HTTP上下文
    int index = client_new_index(client->fd);
    if (index == -1) {
        printf("Error: Maximum clients exceeded\n");
        return;
    }
    client_http_contexts[index] = malloc(sizeof(http_context_t));
    if (!client_http_contexts[index]) {
        printf("Error: Failed to allocate memory for HTTP context\n");
        client_del_index(client->fd);
        return;
    }
    memset(client_http_contexts[index], 0, sizeof(http_context_t));
    client_http_contexts[index]->state = HTTP_STATE_REQUEST_LINE;
    client_http_contexts[index]->headers = dict_create();
    client_http_contexts[index]->request_line = NULL;
    client_http_contexts[index]->method       = NULL;
    client_http_contexts[index]->url_path     = NULL;
    client_http_contexts[index]->content_length = 0;
    client_http_contexts[index]->current_length = 0;
}

void http_cleanup_client(server_t* server, client_context_t* client) {
    int index = client_get_index(client->fd);
    if (index >= 0 && index < MAX_CLIENTS && client_http_contexts[index]) {
        dict_free(&client_http_contexts[index]->headers);
        free(client_http_contexts[index]);
        client_http_contexts[index] = NULL;
        client_del_index(client->fd);
    }
}

static int parse_content_length(dict* headers) {
    char* cl_value = dict_get(headers, "Content-Length");
    if (cl_value) {
        return atoi(cl_value);
    }
    return 0;
}

// 解析HTTP头部行，提取key-value对
static void parse_header_line(dict* headers, char* line) {
    while(*line == ' '){
        line++;
    }
    char* colon = strchr(line, ':');
    if(!colon){
        return;
    }
    *colon = '\0';
    colon++;
    while(*colon == ' '){
        colon++;
    }
    dict_set(headers, line, colon);
}

// 检查路径是否为目录
static int is_directory(const char* path) {
    struct stat path_stat;
    if (stat(path, &path_stat) == 0) {
        return S_ISDIR(path_stat.st_mode);
    }
    return 0;
}

// 函数A：处理目录请求，返回目录列表
static void handle_directory_request(client_context_t* client, const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        // 发送404错误
        const char* response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\n404 Not Found";
        send(client->fd, response, strlen(response), 0);
        return;
    }

    // 构建目录列表HTML
    char* html_content = malloc(4096);
    int pos = 0;
    pos += snprintf(html_content + pos, 4096 - pos, 
        "<html><head><title>Directory listing for %s</title></head><body>\n"
        "<h1>Directory listing for %s</h1>\n<ul>\n", 
        dir_path, dir_path);
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && pos < 3500) {
        if (strcmp(entry->d_name, ".") == 0) continue;

        pos += snprintf(html_content + pos, 4096 - pos,
            "<li><a href=\"%s%s%s\">%s</a></li>\n",
            strcmp(dir_path, ".") == 0 ? "" : dir_path,
            (dir_path[strlen(dir_path)-1] == '/' || strcmp(dir_path, ".") == 0) ? "" : "/",
            entry->d_name, entry->d_name);
    }
    pos += snprintf(html_content + pos, 4096 - pos, "</ul></body></html>");
    closedir(dir);
    char response_header[512];
    snprintf(response_header, sizeof(response_header),
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n", pos);
    send(client->fd, response_header, strlen(response_header), 0);
    send(client->fd, html_content, pos, 0);
    free(html_content);
}

// 处理文件请求，直接发送文件内容
static void handle_file_request(client_context_t* client, const char* file_path) {
    FILE* file = fopen(file_path, "rb");  // 使用二进制模式，避免文本模式的换行符转换
    if (!file) {
        // 发送404错误
        const char* response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\n404 Not Found";
        ssize_t sent = send(client->fd, response, strlen(response), 0);
        if (sent < 0) {
            perror("Failed to send 404 response");
        }
        return;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (file_size < 0) {
        fclose(file);
        const char* response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: 21\r\n\r\n500 Internal Server Error";
        send(client->fd, response, strlen(response), 0);
        return;
    }

    // 发送HTTP头部
    char response_header[512];
    snprintf(response_header, sizeof(response_header),
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %ld\r\n\r\n", file_size);
    ssize_t header_sent = send(client->fd, response_header, strlen(response_header), 0);
    if (header_sent < 0) {
        perror("Failed to send response header");
        fclose(file);
        return;
    }

    // 读取并发送文件内容 - 确保完整发送
    char buffer[8192];
    size_t bytes_read;
    size_t total_sent = 0;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        size_t bytes_to_send = bytes_read;
        char* data_ptr = buffer;
        // 确保每个块都完整发送
        while (bytes_to_send > 0) {
            ssize_t sent = send(client->fd, data_ptr, bytes_to_send, MSG_NOSIGNAL);
            if (sent < 0) {
                perror("Failed to send file data");
                fclose(file);
                return;
            }
            if (sent == 0) {
                printf("Connection closed by client during file transfer\n");
                fclose(file);
                return;
            }
            bytes_to_send -= sent;
            data_ptr += sent;
            total_sent += sent;
        }
    }

    printf("File transfer complete: %zu bytes sent\n", total_sent);
    fclose(file);
}

void http_process_line(server_t* server, client_context_t* client, const char* line, int len) {
    int index = client_get_index(client->fd);
    if (index < 0 || index >= MAX_CLIENTS || !client_http_contexts[index]) {
        return;
    }
    http_context_t* ctx = client_http_contexts[index];
    switch (ctx->state) {
        case HTTP_STATE_REQUEST_LINE:{
            char *request_line = malloc(len + 1);
            if(!request_line) {
                printf("Error: Failed to allocate memory for request line(%d)\n", len);
                return;
            }
            strncpy(request_line, line, len + 1);
            ctx->request_line = request_line;
            char* space1 = strchr(request_line, ' ');
            if (space1) {
                ctx->method = request_line;
                *space1 = '\0';
                space1++;
                ctx->url_path = space1;
                char* space2 = strchr(space1, ' ');
                if (space2) {
                    *space2 = '\0';
                }
            }
            ctx->state = HTTP_STATE_HEADERS;
            break;
        }

        case HTTP_STATE_HEADERS:
            // 空行表示headers结束
            if (len != 0) {
                parse_header_line(&ctx->headers, (char*)line);
                break;
            }
            ctx->content_length = parse_content_length(&ctx->headers);
            if (ctx->content_length <= 0) {
                ctx->state = HTTP_STATE_COMPLETE;
                break;
            }
            ctx->state = HTTP_STATE_BODY;
            ctx = realloc(ctx, sizeof(http_context_t) + ctx->content_length);
            if (!ctx) {
                printf("Error: Failed to allocate memory for body\n");
                return;
            }
            client_http_contexts[index] = ctx;  // 更新全局指针
            break;

        case HTTP_STATE_BODY:
            printf("Receiving body: %d/%d bytes\n", ctx->current_length, ctx->content_length);
            if(ctx->current_length + len + 2 > ctx->content_length) {
                len = ctx->content_length - ctx->current_length; // 防止溢出
            }
            memcpy(ctx->body + ctx->current_length, line, len);
            ctx->current_length += len;
            ctx->body[ctx->current_length++] = '\r';
            ctx->body[ctx->current_length++] = '\n';

            // 检查是否接收完整
            if (ctx->current_length >= ctx->content_length) {
                ctx->state = HTTP_STATE_COMPLETE;
                ctx->body[ctx->content_length] = '\0'; // 添加字符串结束符
            }
            break;

        case HTTP_STATE_COMPLETE:
            ctx->state = HTTP_STATE_REQUEST_LINE;
            break;
    }
    if(ctx->state == HTTP_STATE_COMPLETE) {
        // 处理完整的HTTP请求
        http_handle_complete_request(server, client, ctx);
        http_cleanup_client(server, client);
        http_init_client(server, client);
    }
}

// 处理完整的HTTP请求
void http_handle_complete_request(server_t* server, client_context_t* client, http_context_t* ctx) {
    if (!ctx || !ctx->request_line) {
        const char* response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 15\r\n\r\n400 Bad Request";
        send(client->fd, response, strlen(response), 0);
        return;
    }
    const char* url_path = ctx->url_path;
    if (!url_path) {
        // 发送400错误
        const char* response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 15\r\n\r\n400 Bad Request";
        send(client->fd, response, strlen(response), 0);
        return;
    }

    if (!url_path) {
        const char* response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 15\r\n\r\n400 Bad Request";
        send(client->fd, response, strlen(response), 0);
        return;
    }

    // 处理根路径请求，形成实际的文件系统路径
    char* actual_path = NULL;
    if (strcmp(url_path, "/") == 0 || strcmp(url_path, "") == 0) {
        actual_path = strdup(".");
    } else if (url_path[0] == '/') {
        actual_path = strdup(url_path + 1);
    } else {
        actual_path = strdup(url_path);
    }
    if (!actual_path) {
        const char* response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: 21\r\n\r\n500 Internal Server Error";
        send(client->fd, response, strlen(response), 0);
        return;
    }
    printf("Actual path: %s\n", actual_path);

    // 检查是目录还是文件并处理
    if (is_directory(actual_path)) {
        printf("Handling directory request\n");
        handle_directory_request(client, actual_path);  // 函数A
    } else {
        printf("Handling file request\n");
        handle_file_request(client, actual_path);
    }
    free(actual_path);
}


// 日志函数
void logger(server_t* server, server_log_event_t event, const client_context_t* client) {
    switch (event) {
        case LOG_EVENT_SERVER_START:
            printf("[Log]: Server starting on port %d\n", server->port);
            break;
        case LOG_EVENT_NEW_CLIENT:
            if (client) {
                printf("[Log]: New client connected with fd %d\n", client->fd);
            }
            break;
        case LOG_EVENT_CLIENT_LEFT:
            if (client) {
                printf("[Log]: Client with fd %d disconnected\n", client->fd);
            }
            break;
        case LOG_EVENT_RECEIVE_FAIL:
            if (client) {
                printf("[Log]: Receive failed from client fd %d\n", client->fd);
            }
            break;
        case LOG_EVENT_ACCEPT_FAIL:
            printf("[Log]: Failed to accept new connection\n");
            break;
        default:
            break;
    }
}

int main() {
    server_t server = server_init();
    if (server.error) {
        return 1;
    }
    server_bind(&server, 15445, 'g', 10);
    server_set_newc_hook(&server, http_init_client);
    server_set_delc_hook(&server, http_cleanup_client);
    server_set_rep_func(&server, http_process_line);
    server_set_log_func(&server, logger);

    server_run(&server);

    return 0;
}