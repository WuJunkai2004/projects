#ifndef __CHANNELS_H__
#define __CHANNELS_H__

#include <stdio.h>
#include <sys/types.h>

typedef struct{
    int msgid;
    int isvalid;
} chan;

typedef struct{
    long mtype;
    char mtext[BUFSIZ];
} msg_t;

/**
 * @brief 创建一个消息通道
 * @param id 通道ID
 */
chan* chan_make(key_t id);

/**
 * @brief 释放一个消息通道
 * @param c 通道指针
 */
void  chan_free(chan* c);

/**
 * @brief 发送消息到通道
 * @param c 通道指针
 * @param msg 消息内容
 * @return 成功返回0，失败返回-1
 */
int chan_send_msg(chan* c, const void* msg, int msglen);

/**
 * @brief 从通道接收消息
 * @param c 通道指针
 * @param buf 接收缓冲区
 * @param buflen 缓冲区长度
 * @return 成功返回接收到的字节数，失败返回-1
 */
int chan_recv_msg(chan* c, void* buf, int buflen);


/**
 * @brief 发送字符串消息到通道（以'\0'结尾）
 * @param c 通道指针
 * @param msg 字符串消息
 */
int chan_send_str(chan* c, const char* msg);
int chan_recv_str(chan* c, char* buf, int buflen);


/**
 * @brief 发送结构体消息到通道
 * @param c 通道指针
 * @param struction 结构体变量
 */
#define chan_send(c, struction) \
    chan_send_msg(c, &struction, sizeof(struction))
#define chan_recv(c, struct_t) \
    ({\
        struct_t tmp; \
        int ret = chan_recv_msg(c, &tmp, sizeof(struct_t)); \
        tmp; \
    })


#endif//__CHANNELS_H__