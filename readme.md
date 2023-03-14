# Reactor流程
`IO多路复用(dispatcher)`----`dispatch`---->`事件处理器(handler)`（回调函数封装事件处理循环）
# 架构介绍
Reactor是一种事件驱动的设计模式，并且种类繁多，本项目属于单Reactor多线程模型,为了提高服务器性能，并且充分利用多核处理器，此时，在单Reactor单线程模型的基础上，将`业务处理事件`抽离出`单线程`中，并且插入`线程池`中运行。由于高度封装了Reactor，使得网络IO与业务解耦合，使得在日常开发中能够更加注重与业务开发

| |Reactor|Handlers|
|-|-|-|
|监听事件(accept)|take over(`线程a`)||
|响应事件(read、send)||take over(`线程a`)|
|业务处理事件||take over(`线程池`)|


`核心代码`
```c
    int n = epoll_wait(eventloop->epfd, eventloop->events, MAX_N, -1);
    int i = 0;
    for(i = 0; i < n; i++) {
        struct epoll_event event = eventloop->events[i];
        int mask = 0;
        if(event.events & EPOLLIN) mask |= EPOLLIN;
        if(event.events & EPOLLOUT) mask |= EPOLLOUT;
        //事件分发
        if(mask & EPOLLIN) {
            SockItem* si = event.data.ptr;
            si->callback(si->sockfd, mask, si);
        }
        if(mask & EPOLLOUT) {
            SockItem* si = event.data.ptr;
            si->callback(si->sockfd, mask, si);
        }
    }
```

# 架构图
![图示](https://github.com/DJKarlHarris/MarkdownPhotos/blob/master/reactor.png)

# 改进
由于`监听事件`和`响应事件`处于`同一线程`，当有`大量客户请求`接入时响应事件就无法及时得到处理，并且当有`大量响应事件`触发时，监听事件就无法得到处理

所以可以考虑采用`多Reactor多线程模型`(主从Reactor模型)

在`单Reactor多线程模型`下将`监听事件`与`响应事件`分离，使用`一个Reactor`处理`监听事件`，`多个Reactor`处理`响应事件`
| |Reactor|Handlers|
|-|-|-|
|监听事件(accept)|take over(`线程a`)||
|响应事件(read、send)||take over(`线程b、线程c....`)|
|业务处理事件||take over(`线程池`)|

