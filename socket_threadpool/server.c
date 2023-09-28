#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> // 线程相关的头文件
#include "threadpool.h" // 线程池相关的头文件

struct SockInfo
{
    int fd;
    struct sockaddr_in addr;
};

typedef struct PoolInfo
{
    int fd;
    ThreadPool* p;
}PoolInfo;

void working(void* arg); // 线程的工作函数
void acceptConn(void* arg); // 线程的工作函数

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

    // 创建线程池
    ThreadPool *pool = threadPoolCreate(3, 8, 100);
    PoolInfo* info = (PoolInfo*)malloc(sizeof(PoolInfo));
    info->fd = fd;
    info->p = pool;
    threadPoolAdd(pool, acceptConn, info); // 添加任务

    pthread_exit(NULL);
    return 0;
}

void acceptConn(void* arg){
    // 1. 获取参数
    PoolInfo* poolInfo = (PoolInfo*)arg;
    // 4. 阻塞并等待客户端连接
    int addrlen = sizeof(struct sockaddr_in);
    while (1){    
        struct SockInfo* pinfo = NULL;
        pinfo = (struct SockInfo*)malloc(sizeof(struct SockInfo));
        pinfo->fd = accept(poolInfo->fd, (struct sockaddr*)&pinfo->addr, &addrlen); 
        if(pinfo->fd == -1){
        perror("accept");
        break;
        }
        // 创建通信任务
        threadPoolAdd(poolInfo->p, working, pinfo);
    }

    close(poolInfo->fd);
}

void working(void* arg){

    struct SockInfo* pinfo = (struct SockInfo*)arg;
    // 连接成功，打印客户端的ip和端口
    char ip[32];
    printf("client ip: %s, port: %d\n", 
            inet_ntop(AF_INET, &pinfo->addr.sin_addr.s_addr, ip, sizeof(ip)), 
            ntohs(pinfo->addr.sin_port));

    // 5. 通信
    while (1){
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
}