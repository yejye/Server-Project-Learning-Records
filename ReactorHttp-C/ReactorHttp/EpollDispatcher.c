#include "Dispatcher.h"
#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define Max 520
// 每种模式分发器所需的数据是不同的，需要单独定义
struct EpollData
{
    int epfd;
    struct epoll_event* events;
};
// 使用static将以下函数隐藏：转换为局部函数
// 初始化
static void* epollInit();
// 添加Channel
static int epollAdd(struct Channel* channel, struct EventLoop* evLoop);
// 删除Channel
static int epollRemove(struct Channel* channel, struct EventLoop* evLoop);
// 修改Channel
static int epollModify(struct Channel* channel, struct EventLoop* evLoop);
// 事件监测（启动）
static int epollDispatch(struct EventLoop* evLoop, int timeout); // 单位: s
// 清除数据（关闭fd或释放内存）
static int epollClear(struct EventLoop* evLoop);
// 简化代码
static int epollCtl(struct Channel* channel, struct EventLoop* evLoop, int op);

// Dispatcher 的 Epoll类实例化
//（在源文件中定义的全局变量，可以在外部使用需加extern）
struct Dispatcher EpollDispatcher = {
    epollInit,
    epollAdd,
    epollRemove,
    epollModify,
    epollDispatch,
    epollClear
};

// 初始化，主要是对其数据块data初始化
static void* epollInit()
{
    struct EpollData* data = (struct EpollData*)malloc(sizeof(struct EpollData));
    data->epfd = epoll_create(10);
    if (data->epfd == -1)
    {
        perror("epoll_create");
        exit(0);
    }
    data->events = (struct epoll_event*)calloc(Max, sizeof(struct epoll_event));

    return data;
}

static int epollCtl(struct Channel* channel, struct EventLoop* evLoop, int op)
{
    struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;
    struct epoll_event ev;
    ev.data.fd = channel->fd;
    // 设置检测事件
    int events = 0;
    if (channel->events & ReadEvent)
    {
        events |= EPOLLIN;
    }
    if (channel->events & WriteEvent)
    {
        events |= EPOLLOUT;
    }
    ev.events = events;
    int ret = epoll_ctl(data->epfd, op, channel->fd, &ev);
    return ret;
}

static int epollAdd(struct Channel* channel, struct EventLoop* evLoop)
{ 
    int ret = epollCtl(channel, evLoop, EPOLL_CTL_ADD);
    if (ret == -1)
    {
        perror("epoll_crl add");
        exit(0);
    }
    return ret;
}

static int epollRemove(struct Channel* channel, struct EventLoop* evLoop)
{
    int ret = epollCtl(channel, evLoop, EPOLL_CTL_DEL);
    if (ret == -1)
    {
        perror("epoll_crl delete");
        exit(0);
    }
    // 当前连接断开，cfd相关空间（TcpConnection 资源）释放
    channel->destroyCallback(channel->arg);

    return ret;
}

static int epollModify(struct Channel* channel, struct EventLoop* evLoop)
{
    int ret = epollCtl(channel, evLoop, EPOLL_CTL_MOD);
    if (ret == -1)
    {
        perror("epoll_crl modify");
        exit(0);
    }
    return ret;
}

static int epollDispatch(struct EventLoop* evLoop, int timeout)
{
    struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;
    int count = epoll_wait(data->epfd, data->events, Max, timeout * 1000);
    for (int i = 0; i < count; ++i)
    {
        int events = data->events[i].events;
        int fd = data->events[i].data.fd;
        if (events & EPOLLERR || events & EPOLLHUP)
        {
            // 对方断开了连接, 删除 fd
            // epollRemove(Channel, evLoop);
            epollRemove(evLoop->channelMap->list[fd], evLoop);
            continue;
        }
        if (events & EPOLLIN)
        {
            eventActivate(evLoop, fd, ReadEvent);
        }
        if (events & EPOLLOUT)
        {
            eventActivate(evLoop, fd, WriteEvent);
        }
    }
    return 0;
}

static int epollClear(struct EventLoop* evLoop)
{
    struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;
    free(data->events);
    close(data->epfd);
    free(data);
    return 0;
}
