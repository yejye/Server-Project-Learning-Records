#include "EventLoop.h"
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Log.h"

// 写数据（唤醒）
void taskWakeup(struct EventLoop* evLoop)
{
    const char* msg = "Wake Up!!!";
    write(evLoop->socketPair[0], msg, strlen(msg));
}

// 读数据
int readLocalMessage(void* arg)
{
    struct EventLoop* evLoop = (struct EventLoop*)arg;
    char buf[256];
    read(evLoop->socketPair[1], buf, sizeof(buf));
    return 0;
}

struct EventLoop* eventLoopInit()
{
    return eventLoopInitEx(NULL);
}

struct EventLoop* eventLoopInitEx(const char* threadName)
{
    struct EventLoop* evLoop = (struct EventLoop*)malloc(sizeof(struct EventLoop));
    evLoop->isQuit = false;
    evLoop->threadID = pthread_self();
    pthread_mutex_init(&evLoop->mutex, NULL);
    strcpy(evLoop->threadName, threadName == NULL ? "MainThread" : threadName);
    // 分发器及其数据
    evLoop->dispatcher = &SelectDispatcher;
    evLoop->dispatcherData = evLoop->dispatcher->init();
    // 任务队列
    evLoop->head = evLoop->tail = NULL;
    // channelmap
    evLoop->channelMap = channelMapInit(128);
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, evLoop->socketPair);
    if (ret == -1)
    {
        perror("socketpair");
        exit(0);
    }
    // 指定规则: evLoop->socketPair[0] 发送数据, evLoop->socketPair[1] 接收数据
    struct Channel* channel = channelInit(evLoop->socketPair[1], ReadEvent, 
        readLocalMessage, NULL, NULL, evLoop);
    // channel 添加到任务队列
    eventLoopAddTask(evLoop, channel, ADD);

    return evLoop;
}

int eventLoopRun(struct EventLoop* evLoop)
{
    assert(evLoop != NULL);
    // 取出事件分发和检测模型
    struct Dispatcher* dispatcher = evLoop->dispatcher;
    // 验证当前线程ID是否与EventLoop对应
    if (evLoop->threadID != pthread_self())
    {
        return -1;
    }
    // 循环处理事件
    while (!evLoop->isQuit)
    {
        dispatcher->dispatch(evLoop, 2); // 开启分发器检测，超时2s
        eventLoopProcessTask(evLoop); // 处理任务队列
    }
    return 0;
}

int eventActivate(struct EventLoop* evLoop, int fd, int event)
{
    if (fd < 0 || evLoop == NULL)
    {
        return -1;
    }
    // eventLoop是用来提供channel信息的，查找fd对应回调函数
    struct Channel* channel = evLoop->channelMap->list[fd];
    assert(channel->fd == fd);
    if (event & ReadEvent && channel->readCallback)
    {
        channel->readCallback(channel->arg);
    }
    if (event & WriteEvent && channel->writeCallback)
    {
        channel->writeCallback(channel->arg);
    }
    return 0;
}

int eventLoopAddTask(struct EventLoop* evLoop, struct Channel* channel, int type)
{
    // ！！！注意：任务队列存在主线程与子线程同时访问的情况
    // 因此需要加锁
    pthread_mutex_lock(&evLoop->mutex);
    // 创建新节点
    struct ChannelElement* node = (struct ChannelElement*)malloc(sizeof(struct ChannelElement));
    node->channel = channel;
    node->type = type;
    node->next = NULL;
    // 链表为空
    if (evLoop->head == NULL)
    {
        evLoop->head = evLoop->tail = node;
    }
    else
    {
        evLoop->tail->next = node;  
        evLoop->tail = node;   
    }
    pthread_mutex_unlock(&evLoop->mutex);
    // ！！！新的注意点：任务队列中的任务如何提交给分发器
    /*
        细节：
            1.任务队列中的新任务：可能是当前子线程或者主线程提交的
                1）对原本存在的fd的修改：来自当前子线程
                2）新fd的提交：主线程建立了新的连接
            2.任务队列的处理应该交给子线程，主线程仅用于建立连接
    */
    if (evLoop->threadID == pthread_self())
    {
        // 当前子线程，处理任务队列
        eventLoopProcessTask(evLoop);
    }
    else
    {
        // 当前为主线程——通知子线程处理任务队列中新任务
        // 1.子线程正在工作——工作结束处理任务队列
        // 2.子线程被阻塞：select，poll，epoll ——唤醒子线程
        taskWakeup(evLoop);
    }
    return 0;
}

int eventLoopProcessTask(struct EventLoop* evLoop)
{
    pthread_mutex_lock(&evLoop->mutex);
    // 取出头结点
    struct ChannelElement* head = evLoop->head;
    while (head != NULL)
    {
        struct Channel* channel = head->channel;
        if (head->type == ADD)
        {
            // fd加入分发器
            eventLoopAdd(evLoop, channel);
        }
        else if (head->type == DELETE)
        {
            // fd从分发器中删除
            eventLoopRemove(evLoop, channel);
        }
        else if (head->type == MODIFY)
        {
            // 修改
            eventLoopModify(evLoop, channel);
        }
        struct ChannelElement* tmp = head;
        head = head->next;
        free(tmp);
    }
    // 全部任务处理完成
    evLoop->head = evLoop->tail = NULL;
    pthread_mutex_unlock(&evLoop->mutex);
    return 0;
}

int eventLoopAdd(struct EventLoop* evLoop, struct Channel* channel)
{
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if (fd >= channelMap->size)
    {
        // 当前map容量不足，扩容
        if (!makeMapRoom(channelMap, fd, sizeof(struct Channel*)))
        {
            return -1;
        }
    }
    // 对fd对应位置进行验证，加入map和dispatcher
    if (channelMap->list[fd] == NULL)
    {
        channelMap->list[fd] = channel;
        evLoop->dispatcher->add(channel, evLoop);
    }
    return 0;
}

int eventLoopRemove(struct EventLoop* evLoop, struct Channel* channel)
{
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if (fd >= channelMap->size)
    {
        // 当前map不包含fd
        return -1;
    }
    int ret = evLoop->dispatcher->remove(channel, evLoop);
    return ret;
}

int eventLoopModify(struct EventLoop* evLoop, struct Channel* channel)
{
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if (channelMap->list[fd] == NULL)
    {
        return -1;
    }
    int ret = evLoop->dispatcher->modify(channel, evLoop);
    return ret;
}

int destroyChannel(struct EventLoop* evLoop, struct Channel* channel)
{
    // 删除 channel 和 fd 的对应关系
    evLoop->channelMap->list[channel->fd] = NULL;
    // 关闭 fd
    close(channel->fd);

    // 释放 channel
    free(channel);
    return 0;
}
