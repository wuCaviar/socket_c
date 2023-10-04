#ifndef _THREADPOLL_H_
#define _THREADPOLL_H_

typedef struct ThreadPoll ThreadPoll; //前置声明

/*
    * @function: threadPoolCreate
    * @desc: 创建线程池并初始化
    * @param min: 线程池中最小线程数
    * @param max: 线程池中最大线程数
    * @param queueSize: 线程池中任务队列的最大容量
    * @return: 成功返回线程池地址，失败返回NULL
    */
ThreadPoll* threadPoolCreate(int min, int max, int queueSize);

/*
    * @function: threadPoolDestroy
    * @desc: 销毁线程池
    * @param pool: 线程池地址
    * @return: 成功返回0，失败返回-1
    */
int threadPoolDestroy(ThreadPoll* pool);

/*
    * @function: threadPoolAdd
    * @desc: 向线程池中添加任务
    * @param pool: 线程池地址
    * @param func: 任务函数
    * @param arg: 任务函数参数
    * @return: 成功返回0，失败返回-1
    */
void threadPoolAdd(ThreadPoll* pool, void(*func)(void*), void* arg);

/*
    * @function: threadPoolBusyNum
    * @desc: 获取线程池中工作的线程的个数
    * @param pool: 线程池地址
    * @return: 成功返回线程池中工作的线程的个数，失败返回-1
    */
int threadPoolBusyNum(ThreadPoll* pool);

/*
    * @function: threadPoolAliveNum
    * @desc: 获取线程池中活着的线程的个数
    * @param pool: 线程池地址
    * @return: 成功返回线程池中活着的线程的个数，失败返回-1
    */
int threadPoolAliveNum(ThreadPoll* pool);

/*
    * @function: worker
    * @desc: 工作线程函数
    * @param pool: 线程池地址
    * @return: 无
    */
void* worker(void* arg);

/*
    * @function: manager
    * @desc: 管理者线程函数
    * @param pool: 线程池地址
    * @return: 无
    */
void* manager(void* arg);

/*
    * @function: threadExit
    * @desc: 退出线程
    * @param pool: 线程池地址
    * @return: 无
    */
void threadExit(ThreadPoll* pool);

#endif // _THREADPOOL_H_
