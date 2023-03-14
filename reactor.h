#ifndef _REACTOR_
#define _REACTOR_

#include "ThreadPool/threadpool.h"
#include <sys/epoll.h>
#include <arpa/inet.h>
#define BUFFERSIZE 1024
#define MAX_N 512

typedef struct Reactor{
    int epfd;
    struct epoll_event events[512];
    int stop;
    ThreadPool* threadpool;
}Reactor;

//events：epoll事件
typedef int(*CallBack)(int fd, int events, void* arg);

//事件结构体
typedef struct SockItem {
    int sockfd;
    Reactor* eventloop;

    //事件处理回调
    CallBack callback;

    //用户数据
    char recvbuf[BUFFERSIZE];
    char sendbuf[BUFFERSIZE]; 
    
    //客户套接字信息
    struct sockaddr_in address;
    //数据长度
    int rlen;
    int slen;
}SockItem;

//创建事件处理
SockItem* init_sockitem(Reactor* eventloop, int fd, struct sockaddr_in address, CallBack callback);

//添加epoll事件
int add_event(Reactor* eventloop, int events, SockItem* si);

//删除epoll事件
int del_event(Reactor* eventloop,SockItem* si);

//修改epoll事件
int mod_event(Reactor* eventloop, int events, SockItem* si);

//读回调函数
int recv_cb(int fd, int events, void* arg);

//写回调函数
int send_cb(int fd,int events, void* arg);

//注册事件回调函数
int accept_cb(int fd, int events, void* arg);

//初始化socket
int init_socket(short port, struct sockaddr_in* server_addr);

//初始化reactor
Reactor* init_reactor(int size);

//开启reactor循环
void start_eventloop(Reactor* eventloop);

//停止reactor循环
void stop_eventloop(Reactor* eventloop);

//循环一次
void eventloop_once(Reactor* eventloop);

//回收reactor内存
void release_reactor(Reactor* eventloop);









#endif