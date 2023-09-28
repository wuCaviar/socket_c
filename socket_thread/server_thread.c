#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> // 线程相关的头文件

struct SockInfo
{
    int fd;
    struct sockaddr_in addr;
};

struct SockInfo sockArr[512]; // 保存客户端的socket和ip端口

void* working(void* arg); // 线程的工作函数

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

    // 初始化结构体数组
    int max = sizeof(sockArr) / sizeof(sockArr[0]);
    for(int i=0; i<max; ++i){
        bzero(&sockArr[i], sizeof(sockArr[i]));
        sockArr[i].fd = -1;
    }

    // 4. 阻塞并等待客户端连接
    int addrlen = sizeof(struct sockaddr_in);
    while (1){    
        struct SockInfo* pinfo = NULL;
        for(int i=0; i<max; ++i){
            if(sockArr[i].fd == -1){
                pinfo = &sockArr[i];
                break;
            }
        }

        int cfd = accept(fd, (struct sockaddr*)&sockArr->addr, &addrlen); 
        sockArr->fd = cfd;
        if(cfd == -1){
        perror("accept");
        break;
        }
        // 创建线程
        pthread_t tid;
        pthread_create(&tid, NULL, working, sockArr);
        pthread_detach(tid);
    }

    close(fd);

    return 0;
}

void* working(void* arg){

    struct SockInfo* pinfo = (struct SockInfo*)arg;
    // 连接成功，打印客户端的ip和端口
    char ip[32];
    printf("client ip: %s, port: %d\n", 
            inet_ntop(AF_INET, &pinfo->addr.sin_addr.s_addr, ip, sizeof(ip)), 
            ntohs(pinfo->addr.sin_port));

    // 5. 通信
    while (1)
    {
        char buf[1024];
        int len = recv(pinfo->fd, buf, sizeof(buf), 0); // 阻塞
        if(len > 0){
            printf("client say: %s\n", buf);
            send(pinfo->fd, buf, len, 0);
        }else if(len == 0){
            printf("client closed...\n");
            break;
        }else{
            perror("recv");
            break;
        }
    }
    
    // 6. 关闭socket
    close(pinfo->fd);
    pinfo->fd = -1;

    return NULL;
}