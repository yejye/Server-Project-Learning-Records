#pragma once
#include <stdbool.h>
#include "Dispatcher.h"
#include "ChannelMap.h"
#include <pthread.h>

// Dispatcher 的 三种类型实例化
//（在源文件中定义的全局变量，可以在外部使用需加extern）
extern struct Dispatcher EpollDispatcher;
extern struct Dispatcher PollDispatcher;
extern struct Dispatcher SelectDispatcher;

// 处理任务节点的三种操作
enum ElemType{ADD, DELETE, MODIFY};

// 定义任务队列的节点
struct ChannelElement
{
    int type;   // 如何处理该节点中的channel
    struct Channel* channel;
    struct ChannelElement* next;
};
struct Dispatcher;
struct EventLoop
{
    // 分发器是否退出
    bool isQuit;
    // 分发器及其数据
    struct Dispatcher* dispatcher;
    void* dispatcherData;
    // 任务队列
    struct ChannelElement* head;
    struct ChannelElement* tail;
    // map
    struct ChannelMap* channelMap;
    // 线程id, name, mutex
    pthread_t threadID;
    char threadName[32];
    pthread_mutex_t mutex;
    // 存储本地通信的fd 通过socketpair 初始化（唤醒）
    int socketPair[2];  
};

// 初始化
struct EventLoop* eventLoopInit();
struct EventLoop* eventLoopInitEx(const char* threadName);
// 启动反应堆模型
int eventLoopRun(struct EventLoop* evLoop);
// 处理被激活的fd，调用对应回调函数
int eventActivate(struct EventLoop* evLoop, int fd, int event);
// 任务队列的管理：
// 任务是要交给分发器监控的新fd+event
// 建立了新的连接，需要加入新的Channel
// 向任务队列添加任务：任务可能是对dispatcher进行添加/修改/删除
int eventLoopAddTask(struct EventLoop* evLoop, struct Channel* channel, int type);
// 处理任务队列中的任务
int eventLoopProcessTask(struct EventLoop* evLoop);
// 处理dispatcher中的节点
int eventLoopAdd(struct EventLoop* evLoop, struct Channel* channel);
int eventLoopRemove(struct EventLoop* evLoop, struct Channel* channel);
int eventLoopModify(struct EventLoop* evLoop, struct Channel* channel);
// 释放channel相关资源
int destroyChannel(struct EventLoop* evLoop, struct Channel* channel);

