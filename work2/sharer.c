#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>

#include "sharemem.h"
#include "subprocess.h"

// 两个进程通过共享内存实现聊天

typedef struct {
    volatile int msg_id;    // 消息ID，递增
    pid_t writer_pid;       // 写入者的进程ID
    char message[512];      // 聊天内容
} shared_chat_t;


void handle_exit(int sig){
    exit(0);
}


fork_func(reader,
fork_args(pid_t parent_pid; shared_chat_t* chat;)
){
    set_signal_handler(SIGINT, handle_exit);

    shared_chat_t* chat = this->chat;

    smlock(chat);
    int last_seen_id = chat->msg_id;
    smunlock(chat);

    while(1){
        if(smlock(chat) == -1){
            break;
        }
        if(chat->msg_id > last_seen_id && chat->writer_pid != this->parent_pid){
            printf("\r\33[2K< %s", chat->message);
            printf("> ");
            fflush(stdout);
            last_seen_id = chat->msg_id;
        }
        smunlock(chat);
        usleep(300 * 1000); // 轮询间隔：300 毫秒
    }
}


int main(){
    set_signal_handler(SIGINT, handle_exit);

    key_t key = ftok("/tmp/sm_key_file", 'C');
    if(key == -1){
        perror("ftok (请先手动创建 /tmp/sm_key_file)");
        exit(1);
    }

    pid_t my_pid = getpid();
    shared_chat_t* chat = smalloc(key, sizeof(shared_chat_t));
    if(chat == NULL){
        fprintf(stderr, "smalloc 失败!\n");
        exit(1);
    }

    pid_t child_pid = pfork(reader, my_pid, chat);

    char input_buffer[500];
    printf("[PID %d] 发送者已启动 (子进程 %d 正在后台接收)。\n", my_pid, child_pid);
    printf("输入消息按回车发送 (输入 'exit' 退出):\n> ");
    fflush(stdout);

    while(1){
        if(fgets(input_buffer, sizeof(input_buffer), stdin) == NULL){
            break;
        }
        if(strncmp(input_buffer, "exit", 4) == 0){
            break;
        }

        if(smlock(chat) == -1){
            break;
        }
        strcpy(chat->message, input_buffer);
        chat->writer_pid = my_pid;
        chat->msg_id++;
        smunlock(chat);

        printf("> ");
        fflush(stdout);
    }

    printf("[PID %d] 发送者退出，正在关闭...\n", my_pid);

    post_signal(child_pid, SIGINT);
    pjoin();
    smfree(chat);

    return 0;
}