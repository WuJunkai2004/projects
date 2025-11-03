#ifndef __SERVER_H__
#define __SERVER_H__

#include <stdio.h>

#define MAX_CLIENTS (100)

// "连接"对象，用于存储每个客户端的状态
typedef struct {
    int fd;                     // 客户端的 socket 文件描述符
    char buffer[BUFSIZ];        // 为这个客户端准备的独立接收缓冲区
    int buffer_len;             // 当前缓冲区中数据的长度
} client_context_t;


// Forward declaration
struct server_s;

/**
 * @brief 日志事件类型
 */
typedef enum {
    LOG_EVENT_SERVER_START,
    LOG_EVENT_NEW_CLIENT,
    LOG_EVENT_CLIENT_LEFT,
    LOG_EVENT_RECEIVE_FAIL,
    LOG_EVENT_ACCEPT_FAIL,
} server_log_event_t;


/**
 * @brief 输出日志函数类型
 */
typedef void (*server_log_func_t)(struct server_s* server, server_log_event_t event, const client_context_t* client);

/**
 * @brief 处理函数类型
 */
typedef void (*server_rep_func_t)(struct server_s* server, client_context_t* client, const char* message, int length);

/**
 * @brief 连接周期管理函数类型
 */
typedef void (*server_conn_func_t)(struct server_s* server, client_context_t* client);

typedef struct server_s {
    int error;          // 错误码，0表示无错误
    int s_fd;           // 服务器套接字文件描述符
    int port;           // 服务器监听端口
    server_log_func_t log_func; // 日志输出函数
    server_rep_func_t rep_func; // 请求处理函数
    server_conn_func_t newc_hook; // 新连接钩子函数
    server_conn_func_t delc_hook; // 断开连接钩子函数
} server_t;

/**
 * @brief Initialize the server
 * @return server_t*: server instance pointer
 */
server_t server_init();

/**
 * @brief 将服务器绑定到指定端口并开始监听
 * @param server: 服务器实例
 * @param port: 监听端口号
 * @param mode: 访问模式：'l' local, 'g' global
 * @param backlog: 连接队列长度
 * @return int: 0 on success, -1 on failure
 */
int server_bind(server_t* server, int port, char mode, int backlog);

/**
 * @brief 处理客户端请求
 * @param server: 服务器实例
 * @param func: 处理函数
 */
void server_set_rep_func(server_t* server, server_rep_func_t func);

/**
 * @brief 设置日志输出函数
 * @param server: 服务器实例
 * @param func: 日志输出函数
 */
void server_set_log_func(server_t* server, server_log_func_t func);

/**
 * @brief 设置新连接钩子函数
 * @param server: 服务器实例
 * @param func: 新连接钩子函数
 */
void server_set_newc_hook(server_t* server, server_conn_func_t func);

/**
 * @brief 设置断开连接钩子函数
 * @param server: 服务器实例
 * @param func: 断开连接钩子函数
 */
void server_set_delc_hook(server_t* server, server_conn_func_t func);

/**
 * @brief 服务器主循环，接受并处理客户端连接
 * @param server: 服务器实例
 */
void server_run(server_t* server);


#endif//__SERVER_H__