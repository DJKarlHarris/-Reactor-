#include <pthread.h>
#include <stdio.h>
#include "threadpool.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define KILLNUMBER 2
#define CREATENUMBER 2

//创建线程池并初始化，返回线程池地址
ThreadPool* threadPoolCreate(int min, int max,int size) {

    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do {
        //创建线程池实例
        if(pool == NULL) {
            printf("create pool fail...\n");
            break;
        }

        //创建工作线程数组
        pool->workerID = (pthread_t*)malloc(max * sizeof(pthread_t));
        if(pool->workerID == NULL) {
            printf("create workerID fail...\n");
            break;
        }
        memset(pool->workerID, 0, sizeof(pthread_t) * max);
        pool->minNum = min;
        pool->maxNum = max;
        pool->buzyNum = 0;
        pool->liveNum = min; 
        pool->exitNum = 0;

        //创建任务数组
        pool->taskQueue = (Task*)malloc(sizeof(Task) * size);
        pool->queueCapacity = size;
        pool->queueSize = 0; 
        pool->queueFront = 0;
        pool->queueRear = 0;

        //线程池为就绪状态
        pool->shutdown = 0;

        //初始化互斥锁和条件变量
        if(pthread_mutex_init(&pool->mutexPool, NULL) != 0 || 
        pthread_mutex_init(&pool->mutexBuzyNum, NULL) != 0 ||
        pthread_cond_init(&pool->notFull, NULL) != 0 ||
        pthread_cond_init(&pool->notEmpty, NULL) != 0) {
            printf("mutex or condition creation fail...\n");
            break;
        } 

        //创建线程
        pthread_create(&pool->managerID, NULL, manager, pool);
        for(int i = 0; i < min; i++) {
            pthread_create(&pool->workerID[i], NULL, worker, pool);
        }

        return pool;

    } while(0);
    
    if(pool && pool->taskQueue) {
        free(pool->taskQueue);
    }

    if(pool && pool->workerID) {
        free(pool->workerID);
    }

    if(pool) {
        free(pool);
    }
    //malloc - free
    return NULL;
}

//工作函数
void* worker(void* arg) {
    ThreadPool* pool = (ThreadPool*) arg;
    
    //循环访问任务队列
    while(1) {
        pthread_mutex_lock(&pool->mutexPool);
        //while的原因：若阻塞9个线程，但只有一个资源，那么唤醒一个线程，继续阻塞8个线程
        //阻塞条件：任务队列没有任务，并且pool没有销毁
        while(pool->queueSize == 0 && !pool->shutdown) {
            //阻塞工作线程
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);
            //工作线程自杀
            if(pool->exitNum > 0) {
                //不管线程有没有自杀成功，exitNum都要恢复为0
                pool->exitNum--;
                if(pool->liveNum > pool->minNum) {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    //调用退出线程函数
                    threadExit(pool);                    
                }
            }
        }

        if(pool->shutdown) {
            printf("shutdown exit！\n");
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }

        //取出任务
        Task task;
        task.function = pool->taskQueue[pool->queueFront].function;
        task.arg = pool->taskQueue[pool->queueFront].arg;
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        pool->queueSize--;

        //唤醒生产者线程
        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexPool);

        //忙线程++
        printf("threadID %ld start working\n",pthread_self());
        pthread_mutex_lock(&pool->mutexBuzyNum);
        pool->buzyNum++;
        pthread_mutex_unlock(&pool->mutexBuzyNum);

        //调用任务,释放存在堆内存中的参数
        task.function(task.arg);
        //free(task.arg);
        task.arg = NULL;

        printf("threadID %ld stop working\n",pthread_self());
        //忙线程--
        pthread_mutex_lock(&pool->mutexBuzyNum);
        pool->buzyNum--;
        pthread_mutex_unlock(&pool->mutexBuzyNum);
    }
    return NULL;
    
}

//管理者线程
void* manager(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    while(!pool->shutdown) {
        //每隔三秒检测一次
        sleep(3);

        //获取任务个数、存活线程数
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);

        pthread_mutex_lock(&pool->mutexBuzyNum);
        int buzyNum = pool->buzyNum;
        pthread_mutex_unlock(&pool->mutexBuzyNum);

        //添加线程：每次添加CREATNUMBER个，
        //任务个数>存活线程个数 && 存活线程数 < 最大线程数
        int count = 0;
        if(queueSize > pool->liveNum && liveNum < pool->maxNum) {
            pthread_mutex_lock(&pool->mutexPool);
            //for循环中有pool->liveNum是互斥量，所以锁要上到for循环外部
            for(int i = 0; i < pool->maxNum &&
             pool->liveNum < pool->maxNum &&
              count < CREATENUMBER; i++) {
                if(pool->workerID[i] == 0) {
                    printf("New thread is created!\n");
                    pthread_create(&pool->workerID[i], NULL, worker, pool);
                    count++;
                    pool->liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }

        //这里可能出现问题，如果同时存在添加线程和销毁线程的条件
        //销毁线程
        //忙线程*2<存活线程 && 存活线程 > 最小线程
        //每次删除KILLNUMBER个
        if(buzyNum * 2 < liveNum && liveNum > pool->minNum) {
            //工作线程根据KILLNUMBER来判定自己醒来时需不需要自杀
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = KILLNUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            //每次唤醒KILLNUMBER个
            for(int i = 0; i < KILLNUMBER; i++) {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
        //管理线程不是主动销毁线程，而是让工作线程去自杀

    }
    return NULL;
}

//添加任务函数
void threadPoolAddTask(ThreadPool* pool, void(*func)(void*), void* arg) {
    
    pthread_mutex_lock(&pool->mutexPool);

    //阻塞生产线程
    while(pool->queueSize == pool->queueCapacity && !pool->shutdown) {
        pthread_cond_wait(&pool->notFull, &pool->mutexPool);
    }
    
    if(pool->shutdown == 1) {
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }

    //放入任务
    pool->taskQueue[pool->queueRear].function = func;
    pool->taskQueue[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->queueSize++;

    //唤醒阻塞的工作线程
    pthread_cond_signal(&pool->notEmpty);

    pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBuzyNum(ThreadPool *pool) {
    pthread_mutex_lock(&pool->mutexBuzyNum);
    int buzyNum = pool->buzyNum;
    pthread_mutex_unlock(&pool->mutexBuzyNum);
    return buzyNum;
}

int threadPoolAliveNum(ThreadPool *pool) {
    pthread_mutex_lock(&pool->mutexPool);
    int aliveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return aliveNum;
}

//销毁线程池
int threadPoolDestory(ThreadPool *pool) {
    if(pool == NULL) {
        return -1;
    }

    pool->shutdown = 1;

    //回收管理者线程
    pthread_join(pool->managerID, NULL);

    //销毁工作线程
    //唤醒阻塞的工作线程
    //当shutdown == 1时，醒着的工作线程会自动退出
    for(int i = 0; i < pool->liveNum; i++) {
        pthread_cond_signal(&pool->notEmpty);
    }

    //回收工作线程销毁完毕,保证销毁工作线程在销毁线程池之前
    for(int i = 0; i < pool->maxNum; i++) {
        if(pool->workerID[i] != 0) {
            pthread_join(pool->workerID[i], NULL);
        }
    }
    
    ////如果不采用join的话就要等待工作线程自杀完毕后才释放所有的线程池资源
    //sleep(5);

    //释放堆内存
    if(pool->taskQueue) {
        free(pool->taskQueue);
    }

    if(pool->workerID) {
        free(pool->workerID);
    }

    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBuzyNum);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);

    free(pool);
    pool = NULL;

    return 1;
}
//退出函数修改tid
void threadExit(ThreadPool* pool) {
    pthread_t tid = pthread_self();
    printf("td: %ld \n",tid);
    for(int i = 0; i < pool->maxNum; i++) {
        if(pool->workerID[i] == tid) {
            pool->workerID[i] = 0;
            printf("threadExit() called, threadID:%ld eixiting\n",tid);
            break;
        }
    }
    pthread_exit(NULL);
}