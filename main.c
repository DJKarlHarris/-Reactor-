#include "reactor.h" 
#include <netinet/in.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
struct Reactor* eventloop = NULL;
int main(int argc, char** argv) {

   if(argc < 2) {
        fprintf(stderr, "uasge: %s <port>", argv[0]);
   }
   
   //创建socket
   int servfd = socket(AF_INET, SOCK_STREAM, 0);
   struct sockaddr_in server_addr;

    //绑定socket
   memset(&server_addr, 0, sizeof(server_addr));
   int port = atoi(argv[1]);
   server_addr.sin_port = htons(port);
   server_addr.sin_addr.s_addr = INADDR_ANY;
   server_addr.sin_family = AF_INET;

    if(-1 == bind(servfd, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
        perror("bind");
        return 2;
    }

    //监听
    if(-1 == listen(servfd, 5)) {
        perror("listen");
    }

    
    eventloop = (struct Reactor*)malloc(sizeof(struct Reactor));

    eventloop->epfd = epoll_create(1);
    //注册监听fd
    struct epoll_event event;
    event.events = EPOLLIN;
    struct SockItem* si = (struct SockItem*)malloc(sizeof(struct SockItem));
    si->sockfd = servfd;
    si->callback = accept_cb;

    event.data.ptr = si;
    epoll_ctl(eventloop->epfd, EPOLL_CTL_ADD, si->sockfd, &event);

    while(1) {
        int ready_n = epoll_wait(eventloop->epfd, eventloop->events, 512, -1);
        if(ready_n < -1) {
            break;
        }
        int i = 0;
        for(i = 0; i < ready_n; i++)  {
            if(eventloop->events[i].events & EPOLLIN ) {
                struct SockItem* si = (struct SockItem*)eventloop->events[i].data.ptr;
                si->callback(eventloop, si->sockfd, eventloop->events[i].events, si);
            }

            if(eventloop->events[i].events & EPOLLOUT) {
                struct SockItem* si = (struct SockItem*)eventloop->events[i].data.ptr;
                si->callback(eventloop, si->sockfd, eventloop->events[i].events, si);
            }
        }
    }
    return 0;
    

}