#pragma once
#include "EventLoop.h"
#include "Buffer.h"
#include "Channel.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

// #define MSG_SEND_AUTO 
// 是否将发送设置为写事件触发
// 写事件触发需要准备：回调函数，channel写事件监听，写事件唤醒

// TcpConnection 是一对一管理通信套接字的模块
struct TcpConnection
{
    struct EventLoop* evLoop;
    struct Channel* channel;
    struct Buffer* readBuf;
    struct Buffer* writeBuf;
    char name[32];
    // http 协议
    struct HttpRequest* request;
    struct HttpResponse* response;
};

// 初始化
struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop* evloop);
int tcpConnectionDestroy(void* conn);