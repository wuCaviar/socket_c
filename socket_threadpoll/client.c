#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(){

    // 1. 创建通信的socket
    int fd = socket(AF_INET, SOCK_STREAM, 0); // ipv4, tcp
    if(fd == -1){
        perror("socket");
        return -1;
    }

    // 2. 连接服务器IP port
    struct sockaddr_in saddr; // 服务器的ip和端口
    saddr.sin_family = AF_INET; // ipv4
    saddr.sin_port = htons(9999); // 端口号 转换成大端
    inet_pton(AF_INET, "172.20.10.5", &saddr.sin_addr.s_addr); // 服务器的ip地址
    int ret = connect(fd, (struct sockaddr*)&saddr, sizeof(saddr)); // 绑定
    if(ret == -1){
        perror("connect");
        return -1;
    }

    int number = 0;
    // 3.通信
    while (1)
    {
        // 发送数据
        char buf[1024];
        sprintf(buf, "hello, i am client, %d...\n", number++);
        send(fd, buf, strlen(buf)+1, 0); // 阻塞

        // 接收数据
        memset(buf, 0, sizeof(buf));
        int len = recv(fd, buf, sizeof(buf), 0); // 阻塞
        if(len > 0){
            printf("server say: %s\n", buf);
        }else if(len == 0){
            printf("server closed...\n");
            break;
        }else{
            perror("send");
            break;
        }
        sleep(1);
    }
    
    // 4.关闭socket
    close(fd);

    return 0;
}