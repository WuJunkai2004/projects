#ifndef END_LINE
#define END_LINE '\n'
#endif//END_LINE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <request.h>
#include <errno.h>


int request_create(const char* hostname, int port){
   int sockfd = socket(AF_INET, SOCK_STREAM, 0);	// 创建套接字
   if(sockfd < 0) {
      perror("socket");
      return -1;
   }
   struct sockaddr_in addr = {0}; // 客户端套接字地址
   addr.sin_family = AF_INET;     // 客户端套接字地址中的域
   addr.sin_port = htons(port);   // 客户端套接字端口
   // 尝试解析IP地址
   if( inet_aton(hostname, &addr.sin_addr) == 0 ) {
      // 解析失败，尝试通过域名解析
      struct hostent* host_entry = gethostbyname(hostname);
      if( host_entry == NULL ) {
         perror("gethostbyname");
         close(sockfd);
         return -1;
      }
      memcpy(&addr.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
   }
   if( connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0 ) {
      perror("connect");
      close(sockfd);
      return -1;
   }
   return sockfd;
}

int request_send(const int sockfd, const void* data, size_t length) {
   static char end_line[2] = {END_LINE, 0};
   ssize_t sent = send(sockfd, data, length, 0);
   if(sent < 0) {
      perror("send");
      return -1;
   }
   send(sockfd, end_line, 1, 0); // 发送换行符作为消息结束标志
   return (int)sent;
}

int request_recv(const int sockfd, void* buffer, size_t buffer_size) {
   ssize_t received = recv(sockfd, buffer, buffer_size, 0);
   if(received < 0) {
      perror("recv");
      return -1;
   }
   return (int)received;
}

int request_recv_all(const int sockfd, void* buffer, size_t buffer_size) {
   char* buf = (char*)buffer;
   ssize_t total_received = 0;
   ssize_t received;

   // 设置socket为非阻塞模式，用于检测数据接收完毕
   int flags = fcntl(sockfd, F_GETFL, 0);
   fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

   // 反复接收数据直到没有更多数据
   while(total_received < buffer_size - 1) {
         received = recv(sockfd, buf + total_received, buffer_size - total_received - 1, 0);
         if(received == 0){
            break;
         }
         if(received > 0){
            total_received += received;
            continue;
         }
         // received < 0
         if(errno == EAGAIN || errno == EWOULDBLOCK) {
            // 暂时没有数据，等待一下再试
            usleep(10000); // 等待10ms

            // 检查是否已经接收到完整的HTTP响应
            buf[total_received] = '\0';
            if(total_received > 0 && strstr(buf, "\r\n\r\n")) {
               // 已经有HTTP头部，再等待一点时间看是否有更多数据
               int wait_count = 0;
               while(wait_count < 10) { // 最多等待100ms
                  received = recv(sockfd, buf + total_received,
                                 buffer_size - total_received - 1, 0);
                  if(received == 0){
                     break;
                  }
                  if(received > 0) {
                     total_received += received;
                     wait_count = 0; // 重置等待计数
                     continue;
                  }
                  if(errno == EAGAIN || errno == EWOULDBLOCK) {
                     wait_count++;
                     usleep(10000);
                  } else {
                     break;
                  }
               }
               break;
            }
         } else {
            // 其他错误
            perror("recv");
            break;
         }
      }
      // 恢复socket为阻塞模式
      fcntl(sockfd, F_SETFL, flags);
      buf[total_received] = '\0';
      return (int)total_received;
}

int request_close(int sockfd) {
   return close(sockfd);
}

void raw_add_line(raw_request_t* r, const char* line) {
   int line_len = strlen(line);
   if(r->len + line_len + 2 >= sizeof(r->content)){
      return;  // overflow
   }
   strcpy(r->content + r->len, line);
   r->len += line_len;
   r->content[r->len++] = '\r';
   r->content[r->len++] = '\n';
   r->content[r->len] = 0;
}

int request_send_raw(const int sockfd, raw_request_t* raw) {
   return request_send(sockfd, raw->content, raw->len + 1);
}