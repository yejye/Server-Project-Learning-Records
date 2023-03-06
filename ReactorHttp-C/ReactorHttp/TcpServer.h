#pragma once
#include "EventLoop.h"
#include "ThreadPool.h"

struct Listener
{
    int lfd;
    unsigned short port;
};

// tcpserver 是主线程中启动的服务器
struct TcpServer
{
    int threadNum;
    // 主线程的反应堆
    struct EventLoop* mainLoop;
    // 线程池
    struct ThreadPool* threadPool;
    // 监听部分Listener
    struct Listener* listener;
};

// 初始化
struct TcpServer* tcpServerInit(unsigned short port, int threadNum);
// 初始化监听
struct Listener* listenerInit(unsigned short port);
// 启动服务器
void tcpServerRun(struct TcpServer* server);