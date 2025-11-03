#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <stddef.h>

/**
 * @file request.h
 * @brief 高性能低延迟网络通信模块头文件
 */


/**
 * @brief 请求创建连接
 * @param hostname 服务器主机名或IP地址
 * @param port 服务器端口
 */
int request_create(const char* hostname, int port);


/**
 * @brief 请求发送数据
 * @param data 发送数据缓冲区
 * @param data_len 数据长度
 * @return 成功返回发送的字节数，失败返回-1
 */
int request_send(const int sockfd, const void* data, size_t data_len);


/**
 * @brief 请求接收数据
 * @param buffer 接收数据缓冲区
 * @param buffer_len 缓冲区长度
 */
int request_recv(const int sockfd, void* buffer, size_t buffer_len);
int request_recv_all(const int sockfd, void* buffer, size_t buffer_len);


/**
 * @brief 关闭请求连接
 */
int request_close(int sockfd);


/**
 * @brief raw 支持
 */
typedef struct {
    int len;
    char content[1024 * 16];
} raw_request_t;
void raw_add_line(raw_request_t* r, const char* line);

#define raw_init()              ({raw_request_t* r=malloc(sizeof(raw_request_t)); r->len=0; r->content[0]=0; r;})
#define raw_free(r)             ({free(r);})
#define raw_get(r)              ({(r)->content;})
#define raw_size(r)             ({(r)->len;})


/**
 * @brief HTTP 支持
 */
int request_send_raw(const int sockfd, raw_request_t* raw);

#endif//__REQUEST_H__