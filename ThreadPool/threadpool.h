#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <stdio.h>
#include <pthread.h>

//任务结构体
typedef struct Task {
    void (*function)(void* arg);
    void* arg;
}Task;

//线程池结构体
typedef struct ThreadPool {

    Task* taskQueue;  //任务队列
    int queueCapacity;  //队列最大容量
    int queueSize;  //队列任务个数
    int queueFront; //队头指针
    int queueRear; //队尾指针

    pthread_t* workerID;  //工作线程id数组
    pthread_t managerID; //管理线程id

    int minNum;  //最小线程数
    int maxNum;  //最大线程数
    int buzyNum;  //忙线程数
    int liveNum;  //存活线程数
    int exitNum;  //销毁线程数

    int shutdown;  //线程池是否工作，1为工作，2为销毁

    pthread_mutex_t mutexPool;  //线程池锁
    pthread_mutex_t mutexBuzyNum;  //忙线程变量锁
    pthread_cond_t notFull;  //判断是否为满
    pthread_cond_t notEmpty;  //判断是否为空

}ThreadPool;

//创建线程池
ThreadPool* threadPoolCreate(int min, int max,int size);

//工作函数
void* worker(void* arg);
//管理者函数
void* manager(void* arg);

//获取线程池中工作线程的个数
int threadPoolBuzyNum(ThreadPool* pool);

//获取线程池中活着线程的个数
int threadPoolAliveNum(ThreadPool* pool);

//线程销毁
int threadPoolDestory(ThreadPool* pool);

//退出函数修改tid
void threadExit(ThreadPool* pool);

//添加任务函数
void threadPoolAddTask(ThreadPool* pool, void(*func)(void*), void* arg);


#endif