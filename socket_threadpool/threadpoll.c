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
    ThreadPoll *pool = (ThreadPoll *)malloc(sizeof(ThreadPoll));
    do{
        // 分配内存失败
        if (pool == NULL){ 
            printf("malloc threadpool fail\n");
            break;;
        }

        pool->threadIDs = (pthread_t *)malloc(sizeof(pthread_t) * max); // 创建工作线程ID数组
        if (pool->threadIDs == NULL){
            printf("malloc threadIDs fail\n");
            break;
        }
        memset(pool->threadIDs, 0, sizeof(pthread_t) * max); // 初始化工作线程ID数组
        pool->minNum = min;                                  // 初始化最小线程数
        pool->maxNum = max;                                  // 初始化最大线程数
        pool->busyNum = 0;                                   // 初始化忙线程数
        pool->liveNum = min;                                 // 初始化存活线程数和最小线程数一致
        pool->exitNum = 0;                                   // 初始化要销毁的线程数

        if (pthread_mutex_init(&(pool->mutexPool), NULL) != 0 ||
            pthread_mutex_init(&(pool->mutexBusy), NULL) != 0 ||
            pthread_cond_init(&(pool->notEmpty), NULL) != 0 ||
            pthread_cond_init(&(pool->notFull), NULL) != 0)  // 判断锁和条件变量是否初始化成功
        {
            printf("init the lock or cond fail\n");
            break;
        }

        // 任务队列
        pool->taskQ = (Task *)malloc(sizeof(Task) * queueSize); // 创建任务队列
        pool->queueCapacity = queueSize;                        // 初始化任务队列容量
        pool->queueSize = 0;                                    // 初始化任务队列中实际任务数
        pool->queueFront = 0;                                   // 初始化队头
        pool->queueRear = 0;                                    // 初始化队尾

        pool->shutdown = 0; // 初始化是否要销毁线程池

        // 创建线程
        pthread_create(&(pool->managerID), NULL, manager, pool);        // 创建管理者线程

        for (int i = 0; i < min; ++i) // 按照最小线程数创建工作线程
        {
            pthread_create(&(pool->threadIDs[i]), NULL, worker, pool);  // 创建工作线程
        }
        return pool;                                                    // 返回线程池地址
    } while (0);

    // 线程池创建失败，释放资源
    if (pool && pool->threadIDs) free(pool->threadIDs);                 // 释放工作线程ID数组
    if (pool && pool->taskQ) free(pool->taskQ);                         // 释放任务队列
    if (pool) free(pool);                                               // 释放线程池

    return NULL;
}

int threadPoolDestroy(ThreadPoll* pool){
    if (pool == NULL) return -1;                // 线程池不存在，销毁失败
    
    pool->shutdown = 1;                         // 关闭线程池
    pthread_join(pool->managerID, NULL);        // 阻塞回收管理者线程

    // 唤醒阻塞的工作线程
    for (int i = 0; i < pool->liveNum; ++i){
        pthread_cond_broadcast(&(pool->notEmpty));
    }

    // 释放堆空间
    if (pool->taskQ) free(pool->taskQ);         // 释放任务队列
    if (pool->threadIDs) free(pool->threadIDs); // 释放工作线程ID数组

    pthread_mutex_destroy(&(pool->mutexPool));  // 销毁锁
    pthread_mutex_destroy(&(pool->mutexBusy)); 
    pthread_cond_destroy(&(pool->notEmpty));    // 销毁条件变量
    pthread_cond_destroy(&(pool->notFull));    

    free(pool);                                 // 释放线程池
    pool = NULL;                                // 线程池指针置空
    
    return 0;
}

void threadPoolAdd(ThreadPoll *pool, void (*func)(void *), void *arg){
    pthread_mutex_lock(&(pool->mutexPool)); // 给线程池加锁
    while (pool->queueSize == pool->queueCapacity && !pool->shutdown){
        // 任务队列满，阻塞生产者线程
        pthread_cond_wait(&(pool->notFull), &(pool->mutexPool)); //参数1:条件变量 参数2:互斥锁
    }
    if (pool->shutdown){
        pthread_mutex_unlock(&(pool->mutexPool)); // 给线程池解锁
        return;
    }
    
    // 添加任务
    pool->taskQ[pool->queueRear].function = func; // 在队尾添加任务函数
    pool->taskQ[pool->queueRear].arg = arg;       // 在队尾添加任务函数的参数
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity; // 移动队尾
    pool->queueSize++; // 任务队列中实际任务数加1

    // 唤醒工作线程
    pthread_cond_signal(&(pool->notEmpty));     // 唤醒工作线程
    pthread_mutex_unlock(&(pool->mutexPool));   // 给线程池解锁
}

int threadPoolBusyNum(ThreadPoll *pool){
    pthread_mutex_lock(&(pool->mutexBusy));     // 给busyNum加锁
    int busyNum = pool->busyNum;                // 获取忙线程数
    pthread_mutex_unlock(&(pool->mutexBusy));   // 给busyNum解锁
    return busyNum;
}

int threadPoolAliveNum(ThreadPoll *pool){
    pthread_mutex_lock(&(pool->mutexPool));     // 给线程池加锁
    int liveNum = pool->liveNum;                // 获取存活线程数
    pthread_mutex_unlock(&(pool->mutexPool));   // 给线程池解锁
    return liveNum;
}

void *worker(void *arg){
    ThreadPoll *pool = (ThreadPoll *)arg;       // 获取线程池地址

    while (1)
    {
        pthread_mutex_lock(&(pool->mutexPool)); // 给线程池加锁

        // 判断当前任务队列是否为空
        while (pool->queueSize == 0 && !pool->shutdown)
        {
            //  阻塞工作线程
            pthread_cond_wait(&(pool->notEmpty), &(pool->mutexPool)); // 阻塞工作线程，等待任务队列不为空
            if (pool->exitNum > 0){
                pool->exitNum--;        // 要销毁的线程数减1
                if (pool->liveNum > pool->minNum){
                    pool->liveNum--;    // 存活线程数减1
                    pthread_mutex_unlock(&(pool->mutexPool));         // 给线程池解锁
                    threadExit(pool);   // 销毁线程
                }
            }
        }

        // 判断线程池是否被关闭了
        if (pool->shutdown)
        {
            pthread_mutex_unlock(&(pool->mutexPool));   // 解锁
            threadExit(pool);                           // 退出线程
        }

        // 从任务队列中取出任务
        Task task;
        task.function = pool->taskQ[pool->queueFront].function; // 取出任务函数
        task.arg = pool->taskQ[pool->queueFront].arg;           // 取出任务函数的参数
        // 移动队头
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity; // 移动队头 循环取任务
        pool->queueSize--;                                               // 任务队列中实际任务数减1

        pthread_cond_signal(&(pool->notFull));      // 唤醒生产者线程
        pthread_mutex_unlock(&(pool->mutexPool));   // 给线程池解锁

        printf("thread %ld start working...\n", pthread_self());    //开始工作
        pthread_mutex_lock(&(pool->mutexBusy));     // 给busyNum加锁
        pool->busyNum++;                            // 忙线程数加1
        pthread_mutex_unlock(&(pool->mutexBusy));   // 给busyNum解锁

        task.function(task.arg);                    // 执行任务函数
        free(task.arg);                             //传入堆空间的地址，需要手动释放
        task.arg = NULL;                            //防止野指针
        // (*task.function)(task.arg); // 执行任务函数

        printf("thread %ld end working...\n", pthread_self());      //结束工作
        pthread_mutex_lock(&(pool->mutexBusy));     // 给busyNum加锁
        pool->busyNum--;                            // 忙线程数减1
        pthread_mutex_unlock(&(pool->mutexBusy));   // 给busyNum解锁
    }
    return NULL;
}

void *manager(void *arg){
    ThreadPoll *pool = (ThreadPoll *)arg;           // 获取线程池地址
    while (!pool->shutdown)
    {
        // 每隔3s检测一次
        sleep(3);

        // 取出线程池中任务的数量和当前线程的数量
        pthread_mutex_lock(&(pool->mutexPool));     // 给线程池加锁
        int queueSize = pool->queueSize;            // 获取任务队列中实际任务数
        int liveNum = pool->liveNum;                // 获取存活线程数
        pthread_mutex_unlock(&(pool->mutexPool));   // 给线程池解锁

        // 取出忙线程的数量
        pthread_mutex_lock(&(pool->mutexBusy));     // 给busyNum加锁
        int busyNum = pool->busyNum;                // 获取忙线程数
        pthread_mutex_unlock(&(pool->mutexBusy));   // 给busyNum解锁

        // 添加线程
        // 任务数 > 存活线程数 && 存活线程数 < 最大线程数
        if (queueSize > liveNum - busyNum && liveNum < pool->maxNum){
            pthread_mutex_lock(&(pool->mutexPool)); // 给线程池加锁
            int counter = 0;                        // 记录成功创建的线程个数
            for (int i = 0; 
            i < pool->maxNum                        // 线程池中线程的个数 < 最大线程数
            && counter < NUMBER                     // 成功创建的线程个数 < 线程池中线程的个数
            && pool->liveNum < pool->maxNum;        // 存活线程数 < 最大线程数
            ++i){
                if (pool->threadIDs[i] == 0){
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool); // 创建工作线程
                    counter++;                      // 成功创建的线程个数加1
                    pool->liveNum++;                // 存活线程数加1
                }
            }
            pthread_mutex_unlock(&(pool->mutexPool)); // 给线程池解锁
        }

        // 销毁线程
        // 忙线程 * 2 < 存活线程数 && 存活线程数 > 最小线程数
        if (busyNum*2 < liveNum&& liveNum > pool->minNum) {
            pthread_mutex_lock(&(pool->mutexPool));     // 给线程池加锁
            pool->exitNum = NUMBER; // 要销毁的线程数 = 2
            pthread_mutex_unlock(&(pool->mutexPool));   // 给线程池解锁
            // 让工作的线程自杀 666
            for (int i = 0; i < NUMBER; ++i){
                pthread_cond_signal(&(pool->notEmpty)); // 唤醒工作线程
            }
        }
    }
    return NULL;
}

void threadExit(ThreadPoll *pool){
    pthread_t tid = pthread_self(); // 获取当前线程ID
    for (int i = 0; i < pool->maxNum; ++i)
    {
        // 找到要销毁的线程
        if (pool->threadIDs[i] == tid){
            pool->threadIDs[i] = 0; // 将要销毁的线程ID置为0
            printf("threadExit() called, %ld exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);             // 线程自杀
}
