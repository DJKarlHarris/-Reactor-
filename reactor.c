#include "reactor.h" 
#include <asm-generic/errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>



int accept_cb(struct Reactor* eventloop, int fd, int events, void *arg) {
    struct sockaddr_in cli_addr;
    memset(&cli_addr, 0, sizeof(cli_addr));
    socklen_t cli_len = sizeof(cli_addr);

    int clifd = accept(fd, (struct sockaddr*)&cli_addr, &cli_len);
    if(clifd < 0) { return -1;}

    char ip[INET_ADDRSTRLEN] = {0};
    printf("recv from %s at port %d\n", inet_ntop(AF_INET, &cli_addr.sin_addr,ip, sizeof(ip)), 
        htons(cli_addr.sin_port));
    
    //添加event为IO读事件
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;

    //注册事件处理器
    struct SockItem* si = (struct SockItem*)malloc(sizeof(struct SockItem));
    si->sockfd = clifd;
    si->callback = recv_cb;
    si->address = cli_addr;
    event.data.ptr = si;

    epoll_ctl(eventloop->epfd, EPOLL_CTL_ADD, clifd, &event);
    return clifd;
}

int recv_cb(struct Reactor* eventloop, int fd, int events, void *arg) {
    struct SockItem* si = (struct SockItem*)arg;
    int ret = recv(fd, si->recvbuf, BUFFERSIZE, 0);
    //1.接受到数据 
    //2. 连接关闭
    //3. 读取出错 3.1 若缓冲区无数据直接return 3.2将fd从epoll中删除
    struct epoll_event event;
    if(ret < 0) {
        if(errno == EAGAIN && errno == EWOULDBLOCK) {
            return -1;
        }
        event.events = EPOLLIN;
        epoll_ctl(eventloop->epfd, EPOLL_CTL_DEL, fd, &event);
        close(fd);
        free(si);
    } else if(ret == 0) {
        
        char ip[INET_ADDRSTRLEN];
        printf("disconnect %s\n", inet_ntop(AF_INET, &si->address.sin_addr, ip, sizeof(ip)));
        event.events = EPOLLIN;
        epoll_ctl(eventloop->epfd, EPOLL_CTL_DEL, fd, &event);
        close(fd);
        free(si);
    } else {
        printf("Recv : %s, %d Bytes\n", si->recvbuf, ret);
        si->rlen = ret;
        memcpy(si->sendbuf, si->recvbuf, si->rlen);
        si->slen = si->rlen;

        event.events = EPOLLOUT | EPOLLET;
        si->callback = send_cb;
        si->sockfd = fd;
        event.data.ptr = si;
        epoll_ctl(eventloop->epfd, EPOLL_CTL_MOD, fd, &event);
    } 
    return fd;
}

int send_cb(struct Reactor* eventloop, int fd, int events, void *arg) {
    struct SockItem* si = (struct SockItem*)arg;
    send(fd, si->sendbuf, si->slen, 0);

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    si->callback = recv_cb;
    si->sockfd = fd;
    event.data.ptr = si;

    epoll_ctl(eventloop->epfd, EPOLL_CTL_MOD, fd, &event);
    return fd;
}