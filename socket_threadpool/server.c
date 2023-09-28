#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(){

    // 1. 创建监听的socket
    int fd = socket(AF_INET, SOCK_STREAM, 0); // ipv4, tcp
    if(fd == -1){
        perror("socket");
        return -1;
    }

    // 2. 绑定ip和端口
    struct sockaddr_in saddr; // 服务器的ip和端口
    saddr.sin_family = AF_INET; // ipv4
    saddr.sin_port = htons(9999); // 端口号 转换成大端
    saddr.sin_addr.s_addr = INADDR_ANY; // 0 = 0.0.0.0 服务器的ip地址 读取本机的ip地址
    int ret = bind(fd, (struct sockaddr*)&saddr, sizeof(saddr)); // 绑定
    if(ret == -1){
        perror("bind");
        return -1;
    }

    // 3. 设置监听
    ret = listen(fd, 128); // 最大监听数
    if(ret == -1){
        perror("listen");
        return -1;
    }

    // 4. 阻塞并等待客户端连接
    struct sockaddr_in caddr; // 客户端的ip和端口
    int addrlen = sizeof(caddr);
    int cfd = accept(fd, (struct sockaddr*)&caddr, (socklen_t*)&addrlen); // 阻塞
    if(cfd == -1){
        perror("accept");
        return -1;
    }

    // 连接成功，打印客户端的ip和端口
    char ip[32];
    printf("client ip: %s, port: %d\n", inet_ntop(AF_INET, &caddr.sin_addr.s_addr, ip, sizeof(ip)), ntohs(caddr.sin_port));

    // 5. 通信
    while (1)
    {
        char buf[1024];
        int len = recv(cfd, buf, sizeof(buf), 0); // 阻塞
        if(len > 0){
            printf("client say: %s\n", buf);
            send(cfd, buf, len, 0);
        }else if(len == 0){
            printf("client closed...\n");
            break;
        }else{
            perror("recv");
            break;
        }
    }
    
    // 6. 关闭socket
    close(cfd);
    close(fd);

    return 0;
}