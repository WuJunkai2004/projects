#include "channels.h"

#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>

chan* chan_make(key_t id) {
    chan* c = (chan*)malloc(sizeof(chan));
    if(!c){
        return NULL;
    }
    c->msgid = msgget(id, IPC_CREAT | 0666);
    if(c->msgid == -1){
        free(c);
        return NULL;
    }
    c->isvalid = 1;
    return c;
}

void chan_free(chan* c) {
    if(!c){
        return;
    }
    if(c->isvalid){
        msgctl(c->msgid, IPC_RMID, NULL);
        c->isvalid = 0;
    }
    free(c);
}

int chan_send_msg(chan* c, const void* msg, int msglen) {
    if(!c || !c->isvalid || !msg || msglen <= 0 || msglen >= BUFSIZ){
        return -1;
    }
    msg_t m;
    m.mtype = 1; // 消息类型
    memcpy(m.mtext, msg, msglen);
    return msgsnd(c->msgid, &m, msglen, 0);
}

int chan_recv_msg(chan* c, void* buf, int buflen) {
    if(!c || !c->isvalid || !buf || buflen <= 0){
        return -1;
    }
    msg_t m;
    ssize_t ret = msgrcv(c->msgid, &m, sizeof(m.mtext), 0, 0);
    if(ret == -1){
        return -1;
    }
    int n = (ret < buflen) ? ret : buflen;
    memcpy(buf, m.mtext, n);
    return n;
}


int chan_send_str(chan* c, const char* msg) {
    if(!c || !c->isvalid || !msg){
        return -1;
    }
    int msglen = strlen(msg) + 1; // 包括终止符
    if(msglen > BUFSIZ){
        return -1;
    }
    return chan_send_msg(c, msg, msglen);
}

int chan_recv_str(chan* c, char* buf, int buflen) {
    if(!c || !c->isvalid || !buf || buflen <= 0){
        return -1;
    }
    msg_t m;
    ssize_t ret = msgrcv(c->msgid, &m, sizeof(m.mtext), 0, 0);
    if(ret == -1){
        return -1;
    }
    int n = (ret < buflen) ? ret : buflen - 1; // 留出空间给终止符
    memcpy(buf, m.mtext, n);
    buf[n] = '\0'; // 确保字符串以'\0'结尾
    return n;
}