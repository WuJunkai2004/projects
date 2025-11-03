#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "subprocess.h"
#include "channels.h"

// client 发送字符串，server 接受字符串

// client 负责上传数据
fork_func(client,
fork_args(int msg_id;)){
    chan* chanel = chan_make(this->msg_id);
    char buf[1024] = {};
    while(1){
        printf("client: ");
        fgets(buf, 1024, stdin);
        chan_send_str(chanel, buf);
        if(strncmp(buf, "exit", 4) == 0){
            chan_free(chanel);
            break;
        }
    }
}


// server 负责接受数据
fork_func(server,
fork_args(int msg_id;)){
    chan* chanel = chan_make(this->msg_id);
    char buf[1024] = {};
    while(1){
        chan_recv_str(chanel, buf, 1024);
        printf("server receive: %s", buf);
        if(strncmp(buf, "exit", 4) == 0){
            chan_free(chanel);
            break;
        }
    }
}



int main() {
    char function[1024] = {};
    input:
    printf("Enter function (client or server): ");
    scanf("%s", function);
    if(strcmp(function, "client") == 0){
        pfork(client, 1234);
    } else if(strcmp(function, "server") == 0){
        pfork(server, 1234);
    } else {
        goto input;
    }
    pjoin();
    return 0;
}