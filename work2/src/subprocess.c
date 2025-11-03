
#include "subprocess.h"

#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>


// 用于记录FILE*与pid的映射
typedef struct {
    FILE* fp;
    pid_t pid;
} popen_map_t;
#define MAX_POPEN 32
static popen_map_t popen_map[MAX_POPEN];

FILE* popen(const char* cmd, const char* mode) {
    int pipefd[2] = {};
    // 创建管道
    if(pipe(pipefd) == -1){
        return NULL;
    }
    // 创建子进程
    pid_t pid = fork();
    if(pid < 0){
        return NULL;
    }
    if(pid == 0){
        // 子进程
        if(mode[0] == 'r'){
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
        } else {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
        }
        execl(cmd, (char*)NULL);
        _exit(127); // exec失败
    }
    FILE* fp = NULL;
    if(mode[0] == 'r'){
        close(pipefd[1]);
        fp = fdopen(pipefd[0], "r");
    } else {
        close(pipefd[0]);
        fp = fdopen(pipefd[1], "w");
    }
    // 记录映射
    for(int i = 0; i < MAX_POPEN; ++i){
        if(popen_map[i].fp == NULL){
            popen_map[i].fp = fp;
            popen_map[i].pid = pid;
            break;
        }
    }
    return fp;
}

int pclose(FILE* stream) {
    pid_t pid = -1;
    for(int i = 0; i < MAX_POPEN; ++i){
        if(popen_map[i].fp == stream){
            pid = popen_map[i].pid;
            popen_map[i].fp = NULL;
            popen_map[i].pid = -1;
            break;
        }
    }
    int status = -1;
    fclose(stream);
    if(pid > 0){
        waitpid(pid, &status, 0);
    }
    return status;
}


// 记录pfork产生的子进程pid
#define MAX_PFORK 64
static pid_t pfork_pids[MAX_PFORK];
static int pfork_pid_count = 0;

// pfork注册pid（在__pfork中调用）
static void pfork_register(pid_t pid) {
    if(pid > 0 && pfork_pid_count < MAX_PFORK){
        pfork_pids[pfork_pid_count++] = pid;
    }
}

pid_t __pfork(pfork_func_t func, void* arg) {
    pid_t pid = fork();
    if(pid < 0){
        return -1;
    }
    if(pid == 0){
        // 子进程
        int ret = func(arg);
        exit(ret);
    }
    // 父进程返回子进程pid
    pfork_register(pid);
    return pid;
}

pid_t __pcall(pfork_func_t func, void* arg) {
    return func(arg);
}

// 等待所有pfork子进程完成
void pjoin(void) {
    for(int i = 0; i < pfork_pid_count; ++i){
        if(pfork_pids[i] > 0){
            waitpid(pfork_pids[i], NULL, 0);
        }
    }
    pfork_pid_count = 0;
}


// 设置信号处理函数，用sigaction实现
void set_signal_handler(int signum, void (*handler)(int)) {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signum, &sa, NULL);
}

// 向指定进程发送信号
int post_signal(pid_t pid, int signo) {
    return kill(pid, signo);
}