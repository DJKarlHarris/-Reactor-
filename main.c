#include "reactor.h" 
#include <netinet/in.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include "./ThreadPool/threadpool.h"



int main(int argc, char** argv) {

   if(argc < 2) {
        fprintf(stderr, "uasge: %s <port>", argv[0]);
   }
   
    int port = atoi(argv[1]);
    struct sockaddr_in server_addr;
    int servfd = init_socket(port, &server_addr);

    struct Reactor* eventloop = init_reactor(MAX_N);


    //创建事件处理器
    SockItem* si = init_sockitem(eventloop, servfd, server_addr, accept_cb);

    //添加epoll
    add_event(eventloop, EPOLLIN, si);

    //开启启动循环
    start_eventloop(eventloop);

    //释放reactor
    release_reactor(eventloop);
   
    return 0;
    

}