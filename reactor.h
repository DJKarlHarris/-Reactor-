#ifndef _REACTOR_
#define _REACTOR_

#include <sys/epoll.h>
#include <arpa/inet.h>
#define BUFFERSIZE 1024

struct Reactor{
    int epfd;
    struct epoll_event events[512];
};

//事件结构体
struct SockItem {
    int sockfd;
    //事件处理回调
    int(*callback)(struct Reactor* eventloop, int fd, int events, void* arg);

    //用户数据
    char recvbuf[BUFFERSIZE];
    char sendbuf[BUFFERSIZE]; 
    
    //客户套接字信息
    struct sockaddr_in address;
    //数据长度
    int rlen;
    int slen;
};



//读回调函数
int recv_cb(struct Reactor* eventloop, int fd, int events, void* arg);
//写回调函数
int send_cb(struct Reactor* eventloop, int fd,int events, void* arg);
//注册事件回调函数
int accept_cb(struct Reactor* eventloopint,int fd, int events, void* arg);







#endif