#include "reactor.h" 
#include "ThreadPool/threadpool.h"
#include <asm-generic/errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

void err_exit(const char* msg) {
    fprintf(stderr, "%s:%s\n", msg, strerror(errno));
}

void printDisConnection(SockItem *si) {
    char ip[INET_ADDRSTRLEN] = {0};
    printf("ip %s disconnect!\n", inet_ntop(AF_INET, &si->address.sin_addr, ip, sizeof(ip)));

}

//设置文件为非阻塞
void setnoblock(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    if( -1 == fcntl(fd, F_SETFL, flag | O_NONBLOCK)) {
        err_exit("fcntl");
    }
}

int accept_cb(int fd, int events, void *arg) {
    SockItem* si = (SockItem*)arg;

    struct sockaddr_in cli_addr;
    memset(&cli_addr, 0, sizeof(cli_addr));
    socklen_t cli_len = sizeof(cli_addr);

    int clifd = accept(fd, (struct sockaddr*)&cli_addr, &cli_len);
    //为了防止当前客户阻塞导致其他客户无法得到响应，所以设置为非阻塞
    setnoblock(clifd);
    
    if(clifd == -1) {
        err_exit("accept");
    }

    char ip[INET_ADDRSTRLEN] = {0};
    printf("recv from %s at port %d\n", inet_ntop(AF_INET, &cli_addr.sin_addr,ip, sizeof(ip)), 
        htons(cli_addr.sin_port));
    
    //封装读事件处理器
    SockItem* recv_si = init_sockitem(si->eventloop, clifd, cli_addr,recv_cb);
    add_event(si->eventloop, EPOLLIN | EPOLLET, recv_si);

    return clifd;
}

//业务函数
void taskFunc(void* arg) {
    SockItem* si = (SockItem*)arg;
    char* data = si->recvbuf;

    //处理业务: 此处为数据的运算
    int ptr = 0;
    char op;
    int flag = 0;//区分左操作数和右操作数
    int lhs = 0, rhs = 0;//左操作数&&右操作数
    while(data[ptr]) {
        if(data[ptr] >= '0' && data[ptr] <= '9') {
            if(flag == 0) {
                //获取左操作数
                for(; data[ptr] >='0' && data[ptr] <= '9'; ptr++) {
                    lhs = lhs * 10;
                    lhs += (data[ptr] - '0');
                }
                flag = 1;
                ptr--;
            } else {
                //获取右操作数
                for(; data[ptr] >= '0' && data[ptr] <= '9'; ptr++) {
                    rhs = rhs * 10;
                    rhs += (data[ptr] - '0');
                }
                break;
            }

        } else if(data[ptr] == ' ') {
            ptr++;
            continue;
        } else {
            op = data[ptr];
        }
        ptr++;
    }
    int res = 0;
    switch(op) {
        case '+' : 
            res = lhs + rhs;
            break;
        case '-' : 
            res = lhs - rhs;
            break;
        case '*' :
            res = lhs * rhs;
            break;
        case '/' :
            res = lhs / rhs;
            break;
        default:
            printf("无效操作符！\n");
    }
    printf("%d %c %d = %d\n", lhs, op, rhs, res);
    sprintf(si->sendbuf, "%d %c %d = %d\n", lhs, op, rhs, res);
    si->slen = strlen(si->sendbuf);
    
    //读缓冲区置空
    memset(si->recvbuf, 0, BUFFERSIZE);
    si->rlen = 0;
    //注册写处理器
    si->callback = send_cb;
    mod_event(si->eventloop, EPOLLOUT | EPOLLET, si);
}

int recv_cb(int fd, int events, void *arg) {
    struct SockItem* si = (struct SockItem*)arg;
    //1.接受到数据 
    //2. 连接关闭
    //3. 读取出错 3.1 若缓冲区无数据直接return 3.2将fd从epoll中删除
    int ret = 0;
    while(1) {
        ret = recv(fd, si->recvbuf, BUFFERSIZE, 0);
        if(ret < 0) {
            if(errno == EINTR || errno == EAGAIN) {
                continue;
            }
            //写缓冲区满了
            if(errno == EWOULDBLOCK) {
                break;
            }
            del_event(si->eventloop,si);
            close(fd);
            err_exit("recv");
        } else if (ret == 0) {
            printDisConnection(si);
            del_event(si->eventloop, si);
            close(fd);
            return 0;
        } else {
            break;
        }
    }
    //将业务任务插入到线程池中
    
#if 0
    //recvbuf->sendbuf
    printf("Recv : %s, %d Bytes\n", si->recvbuf, ret);
    si->rlen = ret;
    memcpy(si->sendbuf, si->recvbuf, si->rlen);
    si->slen = si->rlen;

    //清空recvbuf
    memset(si->recvbuf, 0, BUFFERSIZE);

    si->callback = send_cb;
    mod_event(si->eventloop, EPOLLOUT | EPOLLET, si);
#elif 1
    //将任务插入线程池中
    printf("Recv : %s, %d Bytes\n", si->recvbuf, ret);
    threadPoolAddTask(si->eventloop->threadpool, taskFunc, si);

#endif
return 0;
}

int send_cb(int fd, int events, void *arg) {
    struct SockItem* si = (struct SockItem*)arg;
    while(1) {
        int ret = send(fd, si->sendbuf, BUFFERSIZE, 0);
        if(ret == -1) {
            if(errno == EINTR) {
                continue;
            }
            //写缓冲区满
            if(errno == EWOULDBLOCK) {
                break;
            }
        }
        printf("send %d bytes\n", si->slen);
        memset(si->sendbuf, 0, BUFFERSIZE);
        break;
    }
    si->callback = recv_cb;
    mod_event(si->eventloop, EPOLLIN | EPOLLET, si);
    return 0;
}

int init_socket(short port, struct sockaddr_in* server_addr) {
   //创建socket
    int servfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == servfd) {
        perror("socket");
    }

    //套接字数据初始化
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_port = htons(port);
    server_addr->sin_addr.s_addr = INADDR_ANY;
    server_addr->sin_family = AF_INET;

    //绑定
    if(-1 == bind(servfd, (struct sockaddr*)server_addr, sizeof(*server_addr))) {
        perror("bind");
        return 2;
    }

    //监听
    if(-1 == listen(servfd, 5)) {
        perror("listen");
    }

    return servfd;
}

Reactor* init_reactor(int size) {
    Reactor* reactor = (Reactor*)malloc(sizeof(Reactor));
    reactor->epfd = epoll_create(MAX_N);
    reactor->stop = 0;
    reactor->threadpool = threadPoolCreate(2, 10, 100);
    return reactor; 
}

void start_eventloop(Reactor* eventloop) {
    while(!eventloop->stop) {
        eventloop_once(eventloop);
    }
}

void stop_eventloop(Reactor* eventloop) {
    eventloop->stop = 1;
}

void eventloop_once(Reactor* eventloop) {
    int n = epoll_wait(eventloop->epfd, eventloop->events, MAX_N, -1);
    int i = 0;
    for(i = 0; i < n; i++) {
        struct epoll_event event = eventloop->events[i];
        int mask = 0;
        if(event.events & EPOLLIN) mask |= EPOLLIN;
        if(event.events & EPOLLOUT) mask |= EPOLLOUT;
        if(mask & EPOLLIN) {
            SockItem* si = event.data.ptr;
            si->callback(si->sockfd, mask, si);
        }
        if(mask & EPOLLOUT) {
            SockItem* si = event.data.ptr;
            si->callback(si->sockfd, mask, si);
        }
    }
}

void release_reactor(Reactor* eventloop) {
    if(eventloop == NULL) return;
    close(eventloop->epfd);
    threadPoolDestory(eventloop->threadpool);
    free(eventloop);
}


int add_event(Reactor* eventloop, int events, SockItem* si) {
    struct epoll_event event;
    event.events = events;
    event.data.ptr = si;
    if(-1 == epoll_ctl(eventloop->epfd, EPOLL_CTL_ADD, si->sockfd, &event)) {
        err_exit("epoll_ctl_add");
    }
    return 0;
}

int del_event(Reactor* eventloop, SockItem* si) {
    if(si == NULL) {
        return 1;
    }
    if(-1 == epoll_ctl(eventloop->epfd, EPOLL_CTL_DEL, si->sockfd, NULL)) {
        err_exit("epoll_ctl del");
    }
    //释放事件处理器
    free(si);
    return 0;
}

int mod_event(Reactor* eventloop,int events, SockItem* si) {
    if(si == NULL) {
        return 1;
    }
    struct epoll_event event;
    event.events = events;
    event.data.ptr = si;

    if(-1 == epoll_ctl(eventloop->epfd, EPOLL_CTL_MOD, si->sockfd, &event)) {
        err_exit("epoll_ctl MOD");
    }
    return 0;
}

SockItem* init_sockitem(Reactor* eventloop, int fd, struct sockaddr_in address, CallBack callback) {
    SockItem* si = (SockItem*)malloc(sizeof(SockItem));
    si->sockfd = fd;
    si->callback = callback;
    si->eventloop = eventloop;
    si->address = address;
    return si;
}
