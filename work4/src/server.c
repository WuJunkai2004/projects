#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h> // for fcntl
#include <string.h>

#ifndef END_LINE
#define END_LINE '\n'
#endif//END_LINE

/**
 * @brief 初始化服务器（单例模式）
 * @return server_t 服务器实例
 */
server_t server_init(){
    static int is_init = 0;              // 确保只初始化一次的标志
    static server_t instance = {0};      // 静态服务器实例

    // 如果已经初始化过，直接返回已有实例
    if(is_init){
        return instance;
    }
    is_init = 1;

    // 创建TCP套接字
    instance.s_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(instance.s_fd < 0){
        instance.error = -1;
        return instance;
    }

    // 设置套接字选项：允许地址重用，防止"Address already in use"错误
    int opt = 1;
    if(setsockopt(instance.s_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror("Error: set socket option failed");
        close(instance.s_fd);
        instance.error = -1;
        return instance;
    }

    // 初始化其他成员变量
    instance.error = 0;
    instance.port = 0;
    instance.log_func = NULL;    // 日志回调函数
    instance.rep_func = NULL;    // 消息处理回调函数
    instance.newc_hook = NULL;   // 新连接钩子函数
    instance.delc_hook = NULL;   // 断开连接钩子函数
    return instance;
}


/**
 * @brief 将服务器绑定到指定端口并开始监听
 * @param server 服务器实例指针
 * @param port 监听端口号
 * @param mode 访问模式：'l' local, 'g' global
 * @return 成功返回0，失败返回-1
 */
int server_bind(server_t* server, int port, char mode, int backlog){
    // 检查服务器状态是否正常
    if(server->error){
        return -1;
    }

    // 设置服务器地址结构
    struct sockaddr_in s_addr;
    s_addr.sin_family = AF_INET;         // IPv4协议族
    if(mode == 'l'){
        s_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 仅本地回环访问(127.0.0.1)
    } else if(mode == 'g'){
        s_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 允许所有网络接口访问
    } else {
        fprintf(stderr, "Error: invalid mode '%c'. Use 'l' for local or 'g' for global.\n", mode);
        return -1;
    }
    s_addr.sin_port = htons(port);

    // 绑定套接字
    if(bind(server->s_fd, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0){
        perror("Error: bind socket failed");
        server->error = -1;
        close(server->s_fd);
        return -1;
    }

    // 开始监听连接请求
    if(listen(server->s_fd, backlog) < 0){
        perror("Error: listen socket failed");
        close(server->s_fd);
        return -1;
    }
    server->port = port;
    return 0;
}


void server_set_rep_func(server_t* server, server_rep_func_t func){
    if(server->error){
        return;
    }
    server->rep_func = func;
}


void server_set_log_func(server_t* server, server_log_func_t func){
    if(server->error){
        return;
    }
    server->log_func = func;
}


void server_set_newc_hook(server_t* server, server_conn_func_t func) {
    if (server->error) {
        return;
    }
    server->newc_hook = func;
}

void server_set_delc_hook(server_t* server, server_conn_func_t func) {
    if (server->error) {
        return;
    }
    server->delc_hook = func;
}


static inline void server_log(server_t* server, server_log_event_t event, const client_context_t* client) {
    if (server->log_func) {
        server->log_func(server, event, client);
    }
}


/**
 * @brief 添加新客户端到客户端列表
 * @param clients 客户端数组
 * @param client_count 当前客户端数量指针
 * @param new_fd 新客户端的套接字描述符
 */
static void client_add(client_context_t clients[], int* client_count, int new_fd) {
    // 检查是否超出最大客户端数量限制
    if (*client_count >= MAX_CLIENTS) {
        perror("Too many clients! Connection rejected.\n");
        close(new_fd);
        return;
    }

    // 将新套接字设置为非阻塞模式，避免recv/send阻塞
    int flags = fcntl(new_fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(new_fd, F_SETFL, flags | O_NONBLOCK);
    }

    // 初始化客户端上下文
    clients[*client_count].fd = new_fd;
    clients[*client_count].buffer_len = 0;  // 清空接收缓冲区长度
    (*client_count)++;
}

/**
 * @brief 从客户端列表中移除指定客户端
 * @param clients 客户端数组
 * @param client_count 当前客户端数量指针
 * @param index_to_remove 要移除的客户端索引
 */
static void client_del(client_context_t clients[], int* client_count, int index_to_remove) {
    // 关闭客户端套接字
    close(clients[index_to_remove].fd);

    // 使用"交换删除"法：将最后一个元素移动到被删除位置，避免数组空洞
    // 这样删除操作的时间复杂度为O(1)
    if (index_to_remove < *client_count - 1) {
        clients[index_to_remove] = clients[*client_count - 1];
    }
    (*client_count)--;
}


/**
 * @brief 处理客户端缓冲区中的消息
 * 按行处理数据，每行调用一次回调函数
 * 复杂协议（如HTTP）应在应用层实现状态机
 * @param server 服务器实例指针
 * @param client 客户端上下文指针
 */
void process_client_buffer(server_t* server, client_context_t* client) {
    char* buffer = client->buffer;      // 客户端接收缓冲区
    int* len = &client->buffer_len;     // 缓冲区中数据长度
    char* line_end;

    // 循环处理缓冲区中所有完整的行 (以END_LINE结尾)
    while ((line_end = memchr(buffer, END_LINE, *len)) != NULL) {
        int line_len = line_end - buffer;  // 计算行长度（不包括END_LINE）

        // 提取一行到临时数组中
        char line[line_len + 1];
        memcpy(line, buffer, line_len);
        line[line_len] = '\0';         // 添加字符串结束符

        // 移除\r字符（如果END_LINE是回车）
        if (END_LINE == '\n' && line_len > 0 && line[line_len - 1] == '\r') {
            line[line_len - 1] = '\0';
        }

        // 调用业务逻辑处理回调函数（传递单行数据）
        server->rep_func(server, client, line, line_len);

        // 从缓冲区中移除已处理的行（包括END_LINE）
        *len -= (line_len + 1);
        buffer += (line_len + 1);   // 将缓冲区指针移动到下一行
    }
    if(*len > 0){
        char line[*len + 1];
        memcpy(line, buffer, *len);
        line[*len] = '\0';
        server->rep_func(server, client, line, strlen(line));
        *len = 0;
    }
}


/**
 * @brief 服务器主循环，使用select多路复用处理客户端连接
 * @param server 服务器实例指针
 */
void server_run(server_t* server) {
    // 检查服务器状态
    if (server->error || server->s_fd < 0) {
        perror("Server is not properly initialized.\n");
        return;
    }
    if (server->rep_func == NULL) {
        perror("No message handler function set. Use server_set_rep_func to set one.\n");
        return;
    }

    // 客户端管理相关变量
    client_context_t clients[MAX_CLIENTS];  // 客户端连接数组
    int client_count = 0;                   // 当前客户端数量
    int clients_to_remove[MAX_CLIENTS];     // 待移除客户端索引数组
    int remove_count = 0;                   // 待移除客户端数量

    // select多路复用相关变量
    fd_set read_fds;    // 用于select的读文件描述符集合
    int max_fd;         // select需要监控的最大文件描述符号

    // 触发服务器启动日志事件
    server_log(server, LOG_EVENT_SERVER_START, NULL);

    while (1) {
        FD_ZERO(&read_fds); // 清空集合
        FD_SET(server->s_fd, &read_fds); // 1. 将监听socket加入监控
        max_fd = server->s_fd;

        // 2. 将所有活跃的客户端socket加入监控
        for (int i = 0; i < client_count; i++) {
            FD_SET(clients[i].fd, &read_fds);
            if (clients[i].fd > max_fd) {
                max_fd = clients[i].fd;
            }
        }

        // 3. 调用 select，程序会在这里阻塞，直到有事件发生
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select error");
            break;
        }

        if (FD_ISSET(server->s_fd, &read_fds)) {
            struct sockaddr_in c_addr;
            socklen_t c_len = sizeof(c_addr);
            int new_socket = accept(server->s_fd, (struct sockaddr *)&c_addr, &c_len);
            if (new_socket < 0) {
                perror("accept");
                server_log(server, LOG_EVENT_ACCEPT_FAIL, NULL);
            } else {
                client_add(clients, &client_count, new_socket);
                server_log(server, LOG_EVENT_NEW_CLIENT, &clients[client_count - 1]);
                if(server->newc_hook) {
                    server->newc_hook(server, &clients[client_count - 1]);
                }
            }
        }

        // 重置待移除客户端计数器
        remove_count = 0;

        // 遍历所有客户端，检查是否有可读事件
        for (int i = 0; i < client_count; i++) {
            if (!FD_ISSET(clients[i].fd, &read_fds)) {
                continue;
            }
            // 从客户端接收数据到缓冲区
            ssize_t bytes_read = recv(clients[i].fd,
                                    clients[i].buffer + clients[i].buffer_len,
                                    BUFSIZ - clients[i].buffer_len - 1,  // 预留1字节防止溢出
                                    0);

            if (bytes_read > 0) {
                // 更新缓冲区数据长度
                clients[i].buffer_len += bytes_read;
                // 处理缓冲区中的完整消息
                process_client_buffer(server, &clients[i]);
            } else {
                // bytes_read <= 0：连接关闭或出错
                if (bytes_read < 0) {
                    perror("recv failed");
                    server_log(server, LOG_EVENT_RECEIVE_FAIL, &clients[i]);
                }
                // 标记该客户端需要移除
                clients_to_remove[remove_count++] = i;
            }
        }

        // 倒序移除标记的客户端（避免索引混乱）
        for (int i = remove_count - 1; i >= 0; i--) {
            int client_index = clients_to_remove[i];
            server_log(server, LOG_EVENT_CLIENT_LEFT, &clients[client_index]);
            if(server->delc_hook) {
                server->delc_hook(server, &clients[client_index]);
            }
            client_del(clients, &client_count, client_index);
        }
    }
}