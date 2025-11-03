#ifndef SUBPROCESS_H
#define SUBPROCESS_H

#include <signal.h>
#include <stdio.h>
#include <pthread.h>

#include <sys/types.h>

/**
 * @brief popen: 打开一个进程并创建管道
 * @param cmd: 要执行的命令
 * @param mode: "r" 读取管道, "w" 写入管道
 * @return FILE* 管道文件指针
 */
FILE* popen(const char* cmd, const char* mode);
int   pclose(FILE* stream);



/**
 * @brief __pfork: 内部辅助函数，用 fork 创建子进程并执行指定函数。
 * @param func: 子进程中执行的函数指针。
 * @param arg:  传递给 func 的参数。
 * @return 成功则返回子进程的 PID，失败则返回 -1。
 */
typedef int (*pfork_func_t)(void*);
pid_t __pfork(pfork_func_t func, void* arg);
pid_t __pcall(pfork_func_t func, void* arg);

/**
 * @brief fork_func: 定义一个可以在子进程中执行的函数及其参数结构体。
 * @param name: 函数的名称。
 * @param ...:  函数的参数列表，这里必须是C语言结构体成员的定义语法
 */
#define fork_func(name, ...) \
    typedef struct { __VA_ARGS__ } name##_args_t; \
    static int name##_impl(name##_args_t* this); \
    static int name##_bridge(void* _args){ \
        name##_args_t* args = (name##_args_t*)_args; \
        return name##_impl(args); \
    } \
    static int name##_impl(name##_args_t* this)

/**
 * @brief pfork: 创建一个子进程来执行由 fork_func 定义的函数。
 * @param name: 函数的名称
 * @param ...:  传递给函数的实际参数值，使用C语言的聚合初始化语法。
 * @return 成功则返回子进程的 PID，失败则返回 -1。
 */
#define pfork(name, ...) \
    ({ \
        name##_args_t _args = { __VA_ARGS__ }; \
        __pfork(name##_bridge, &_args); \
    })
#define pcall(name, ...) \
    ({ \
        name##_args_t _args = { __VA_ARGS__ }; \
        __pcall(name##_bridge, &_args); \
    })
#define fork_args(...) __VA_ARGS__
#define fork_loop() do{pause();}while(1)


/**
 * @brief pjoin: 等待所有由 pfork 创建的子进程结束。
 */
void pjoin();


/**
 * @brief set_signal_handler: 设置信号处理函数
 * @param signum: 信号编号，如SIGINT、SIGCHLD等
 * @param handler: 信号处理函数 void handler(int)
 */
void set_signal_handler(int signum, void (*handler)(int));


/**
 * @brief post_signal: 向指定进程发送信号
 * @param pid: 目标进程pid
 * @param signo: 信号编号（如SIGUSR1、SIGUSR2）
 * @return 0成功，-1失败
 */
int post_signal(pid_t pid, int signo);

#endif // SUBPROCESS_H
