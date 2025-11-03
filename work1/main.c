#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "subprocess.h"

#define SIGN_listdir SIGUSR1
#define SIGN_exit    SIGUSR2

void handler_listdir(int signo) {
    printf("========\n子进程收到信号，执行listdir操作\n");
    FILE* fp = popen("/bin/ls", "r");
    printf("当前文件夹有:\n");
    if (!fp) {
        return;
    }
    char buf[256];
    while(fgets(buf, sizeof(buf), fp)){
        fputs(buf, stdout);
    }
    pclose(fp);
}
void handler_exit(int signo) {
    printf("========\n子进程收到信号，准备退出\n");
    _exit(0);
}

fork_func(child) {
    printf("子进程启动，pid=%d\n", getpid());
    set_signal_handler(SIGN_listdir, handler_listdir);
    set_signal_handler(SIGN_exit, handler_exit);
    fork_loop();
    printf("子进程#%d退出\n", getpid());
    return 0;
}

int main() {
    int pid = pfork(child);
    sleep(1);

    printf("父进程发送listdir信号给子进程#%d\n", pid);
    post_signal(pid, SIGN_listdir);
    sleep(1);

    printf("父进程发送exit信号给子进程#%d\n", pid);
    post_signal(pid, SIGN_exit);
    sleep(1);

    printf("父进程退出\n");
    return 0;
}