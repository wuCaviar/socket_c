#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "threadpoll.h"

const int NUMBER = 2;               // 线程池中线程的个数

//任务结构体
typedef struct Task{
    void (*function)(void* arg);    //任务函数
    void* arg;                      //传入任务函数的参数
}Task;

//线程池结构体
struct ThreadPoll{
    Task* taskQ;                    //任务队列
    int queueCapacity;              //任务队列容量
    int queueSize;                  //任务队列中实际任务数
    int queueFront;                 //队头 -> 存数据
    int queueRear;                  //队尾 -> 取数据

    pthread_t managerID;            //管理者线程ID
    pthread_t* threadIDs;           //工作线程ID
    int minNum;                     //最小线程数
    int maxNum;                     //最大线程数
    int busyNum;                    //忙线程数
    int liveNum;                    //存活线程数
    int exitNum;                    //要销毁的线程数

    pthread_mutex_t mutexPool;      //锁整个线程池
    pthread_mutex_t mutexBusy;      //锁busyNum变量

    pthread_cond_t notFull;         //任务队列满，添加任务的线程阻塞
    pthread_cond_t notEmpty;        //任务队列空，取任务的线程阻塞

    int shutdown;                   //是否要销毁线程池 1:销毁 0:不销毁
};

ThreadPoll* threadPoolCreate(int min, int max, int queueSize){
    //创建线程池并初始化
    ThreadPoll *poll = (ThreadPoll *)malloc(sizeof(ThreadPoll));
    do{
        // 分配内存失败
        if (poll == NULL){ 
            printf("malloc threadpool fail\n");
            break;;
        }

        poll->threadIDs = (pthread_t *)malloc(sizeof(pthread_t) * max); // 创建工作线程ID数组
        if (poll->threadIDs == NULL){
            printf("malloc threadIDs fail\n");
            break;
        }
        memset(poll->threadIDs, 0, sizeof(pthread_t) * max); // 初始化工作线程ID数组
        poll->minNum = min;                                  // 初始化最小线程数
        poll->maxNum = max;                                  // 初始化最大线程数
        poll->busyNum = 0;                                   // 初始化忙线程数
        poll->liveNum = min;                                 // 初始化存活线程数和最小线程数一致
        poll->exitNum = 0;                                   // 初始化要销毁的线程数

        if (pthread_mutex_init(&(poll->mutexPool), NULL) != 0 ||
            pthread_mutex_init(&(poll->mutexBusy), NULL) != 0 ||
            pthread_cond_init(&(poll->notEmpty), NULL) != 0 ||
            pthread_cond_init(&(poll->notFull), NULL) != 0)  // 判断锁和条件变量是否初始化成功
        {
            printf("init the lock or cond fail\n");
            break;
        }

        // 任务队列
        poll->taskQ = (Task *)malloc(sizeof(Task) * queueSize); // 创建任务队列
        poll->queueCapacity = queueSize;                        // 初始化任务队列容量
        poll->queueSize = 0;                                    // 初始化任务队列中实际任务数
        poll->queueFront = 0;                                   // 初始化队头
        poll->queueRear = 0;                                    // 初始化队尾

        poll->shutdown = 0; // 初始化是否要销毁线程池

        // 创建线程
        pthread_create(&(poll->managerID), NULL, manager, poll);        // 创建管理者线程

        for (int i = 0; i < min; ++i) // 按照最小线程数创建工作线程
        {
            pthread_create(&(poll->threadIDs[i]), NULL, worker, poll);  // 创建工作线程
        }
        return poll;                                                    // 返回线程池地址
    } while (0);

    // 线程池创建失败，释放资源
    if (poll && poll->threadIDs) free(poll->threadIDs);                 // 释放工作线程ID数组
    if (poll && poll->taskQ) free(poll->taskQ);                         // 释放任务队列
    if (poll) free(poll);                                               // 释放线程池

    return NULL;
}

int threadPoolDestroy(ThreadPoll* poll){
    if (poll == NULL) return -1;                // 线程池不存在，销毁失败
    
    poll->shutdown = 1;                         // 关闭线程池
    pthread_join(poll->managerID, NULL);        // 阻塞回收管理者线程

    // 唤醒阻塞的工作线程
    for (int i = 0; i < poll->liveNum; ++i){
        pthread_cond_broadcast(&(poll->notEmpty));
    }

    // 释放堆空间
    if (poll->taskQ) free(poll->taskQ);         // 释放任务队列
    if (poll->threadIDs) free(poll->threadIDs); // 释放工作线程ID数组

    pthread_mutex_destroy(&(poll->mutexPool));  // 销毁锁
    pthread_mutex_destroy(&(poll->mutexBusy)); 
    pthread_cond_destroy(&(poll->notEmpty));    // 销毁条件变量
    pthread_cond_destroy(&(poll->notFull));    

    free(poll);                                 // 释放线程池
    poll = NULL;                                // 线程池指针置空
    
    return 0;
}

void threadPoolAdd(ThreadPoll *poll, void (*func)(void *), void *arg){
    pthread_mutex_lock(&(poll->mutexPool)); // 给线程池加锁
    while (poll->queueSize == poll->queueCapacity && !poll->shutdown){
        // 任务队列满，阻塞生产者线程
        pthread_cond_wait(&(poll->notFull), &(poll->mutexPool)); //参数1:条件变量 参数2:互斥锁
    }
    if (poll->shutdown){
        pthread_mutex_unlock(&(poll->mutexPool)); // 给线程池解锁
        return;
    }
    
    // 添加任务
    poll->taskQ[poll->queueRear].function = func; // 在队尾添加任务函数
    poll->taskQ[poll->queueRear].arg = arg;       // 在队尾添加任务函数的参数
    poll->queueRear = (poll->queueRear + 1) % poll->queueCapacity; // 移动队尾
    poll->queueSize++; // 任务队列中实际任务数加1

    // 唤醒工作线程
    pthread_cond_signal(&(poll->notEmpty));     // 唤醒工作线程
    pthread_mutex_unlock(&(poll->mutexPool));   // 给线程池解锁
}

int threadPoolBusyNum(ThreadPoll *poll){
    pthread_mutex_lock(&(poll->mutexBusy));     // 给busyNum加锁
    int busyNum = poll->busyNum;                // 获取忙线程数
    pthread_mutex_unlock(&(poll->mutexBusy));   // 给busyNum解锁
    return busyNum;
}

int threadPoolAliveNum(ThreadPoll *poll){
    pthread_mutex_lock(&(poll->mutexPool));     // 给线程池加锁
    int liveNum = poll->liveNum;                // 获取存活线程数
    pthread_mutex_unlock(&(poll->mutexPool));   // 给线程池解锁
    return liveNum;
}

void *worker(void *arg){
    ThreadPoll *poll = (ThreadPoll *)arg;       // 获取线程池地址

    while (1)
    {
        pthread_mutex_lock(&(poll->mutexPool)); // 给线程池加锁

        // 判断当前任务队列是否为空
        while (poll->queueSize == 0 && !poll->shutdown)
        {
            //  阻塞工作线程
            pthread_cond_wait(&(poll->notEmpty), &(poll->mutexPool)); // 阻塞工作线程，等待任务队列不为空
            if (poll->exitNum > 0){
                poll->exitNum--;        // 要销毁的线程数减1
                if (poll->liveNum > poll->minNum){
                    poll->liveNum--;    // 存活线程数减1
                    pthread_mutex_unlock(&(poll->mutexPool));         // 给线程池解锁
                    threadExit(poll);   // 销毁线程
                }
            }
        }

        // 判断线程池是否被关闭了
        if (poll->shutdown)
        {
            pthread_mutex_unlock(&(poll->mutexPool));   // 解锁
            threadExit(poll);                           // 退出线程
        }

        // 从任务队列中取出任务
        Task task;
        task.function = poll->taskQ[poll->queueFront].function; // 取出任务函数
        task.arg = poll->taskQ[poll->queueFront].arg;           // 取出任务函数的参数
        // 移动队头
        poll->queueFront = (poll->queueFront + 1) % poll->queueCapacity; // 移动队头 循环取任务
        poll->queueSize--;                                               // 任务队列中实际任务数减1

        pthread_cond_signal(&(poll->notFull));      // 唤醒生产者线程
        pthread_mutex_unlock(&(poll->mutexPool));   // 给线程池解锁

        printf("thread %ld start working...\n", pthread_self());    //开始工作
        pthread_mutex_lock(&(poll->mutexBusy));     // 给busyNum加锁
        poll->busyNum++;                            // 忙线程数加1
        pthread_mutex_unlock(&(poll->mutexBusy));   // 给busyNum解锁

        task.function(task.arg);                    // 执行任务函数
        free(task.arg);                             //传入堆空间的地址，需要手动释放
        task.arg = NULL;                            //防止野指针
        // (*task.function)(task.arg); // 执行任务函数

        printf("thread %ld end working...\n", pthread_self());      //结束工作
        pthread_mutex_lock(&(poll->mutexBusy));     // 给busyNum加锁
        poll->busyNum--;                            // 忙线程数减1
        pthread_mutex_unlock(&(poll->mutexBusy));   // 给busyNum解锁
    }
    return NULL;
}

void *manager(void *arg){
    ThreadPoll *poll = (ThreadPoll *)arg;           // 获取线程池地址
    while (!poll->shutdown)
    {
        // 每隔3s检测一次
        sleep(3);

        // 取出线程池中任务的数量和当前线程的数量
        pthread_mutex_lock(&(poll->mutexPool));     // 给线程池加锁
        int queueSize = poll->queueSize;            // 获取任务队列中实际任务数
        int liveNum = poll->liveNum;                // 获取存活线程数
        pthread_mutex_unlock(&(poll->mutexPool));   // 给线程池解锁

        // 取出忙线程的数量
        pthread_mutex_lock(&(poll->mutexBusy));     // 给busyNum加锁
        int busyNum = poll->busyNum;                // 获取忙线程数
        pthread_mutex_unlock(&(poll->mutexBusy));   // 给busyNum解锁

        // 添加线程
        // 任务数 > 存活线程数 && 存活线程数 < 最大线程数
        if (queueSize > liveNum - busyNum && liveNum < poll->maxNum){
            pthread_mutex_lock(&(poll->mutexPool)); // 给线程池加锁
            int counter = 0;                        // 记录成功创建的线程个数
            for (int i = 0; 
            i < poll->maxNum                        // 线程池中线程的个数 < 最大线程数
            && counter < NUMBER                     // 成功创建的线程个数 < 线程池中线程的个数
            && poll->liveNum < poll->maxNum;        // 存活线程数 < 最大线程数
            ++i){
                if (poll->threadIDs[i] == 0){
                    pthread_create(&poll->threadIDs[i], NULL, worker, poll); // 创建工作线程
                    counter++;                      // 成功创建的线程个数加1
                    poll->liveNum++;                // 存活线程数加1
                }
            }
            pthread_mutex_unlock(&(poll->mutexPool)); // 给线程池解锁
        }

        // 销毁线程
        // 忙线程 * 2 < 存活线程数 && 存活线程数 > 最小线程数
        if (busyNum*2 < liveNum&& liveNum > poll->minNum) {
            pthread_mutex_lock(&(poll->mutexPool));     // 给线程池加锁
            poll->exitNum = NUMBER; // 要销毁的线程数 = 2
            pthread_mutex_unlock(&(poll->mutexPool));   // 给线程池解锁
            // 让工作的线程自杀 666
            for (int i = 0; i < NUMBER; ++i){
                pthread_cond_signal(&(poll->notEmpty)); // 唤醒工作线程
            }
        }
    }
    return NULL;
}

void threadExit(ThreadPoll *poll){
    pthread_t tid = pthread_self(); // 获取当前线程ID
    for (int i = 0; i < poll->maxNum; ++i)
    {
        // 找到要销毁的线程
        if (poll->threadIDs[i] == tid){
            poll->threadIDs[i] = 0; // 将要销毁的线程ID置为0
            printf("threadExit() called, %ld exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);             // 线程自杀
}
