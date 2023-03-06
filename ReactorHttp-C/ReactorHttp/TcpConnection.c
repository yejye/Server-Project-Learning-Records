#include "TcpConnection.h"
#include "HttpRequest.h"
#include <stdlib.h>
#include <stdio.h>
#include "Log.h"

// cfd读事件对应回调函数
int processRead(void* arg)
{
    // 接收套接字信息，写到buffer中
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    int count = bufferSocketRead(conn->readBuf, conn->channel->fd);
    Debug("接收到的http请求数据: %s  name = %s", conn->readBuf->data + conn->readBuf->readPos, conn->name);
    if (count > 0)
    {
        // 成功接收http请求，解析http请求
        int socket = conn->channel->fd;
#ifdef MSG_SEND_AUTO
        // 监控fd对应写事件：完全写到writebuf才会发送
        // 注意：并不会立刻触发写事件，因为监控与处理属于同一线程，目前线程正在处理
        writeEventEnable(conn->channel, true);
        eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
#endif
        // 解析请求并组织响应发送
        bool flag = parseHttpRequest(conn->request, conn->readBuf, conn->response, conn->writeBuf, socket);
        if (!flag)
        {
            // 解析失败, 回复一个简单的html
            char* errMsg = "Http/1.1 400 Bad Request\r\n\r\n";
            bufferAppendString(conn->writeBuf, errMsg);
        }
    }
    else
    {
        // 触发写事件发送完成，会自动进行资源释放，此处仅针对读取失败的情况
#ifdef MSG_SEND_AUTO
        // 断开连接: 写事件触发发送，不能在函数最后直接断开连接
        // 因为此函数执行完毕，才会进行事件监控，一旦断开连接，channel等均被释放
        eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
#endif
    }
#ifndef MSG_SEND_AUTO
    // 断开连接：如果边存边发，此时已完成发送，直接断开连接
    eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
#endif
    return 0;
}

// cfd写事件对应回调函数
int processWrite(void* arg)
{
    Debug("开始发送数据了(基于写事件发送)....");
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    // 发送数据
    int count = bufferSendData(conn->writeBuf, conn->channel->fd);
    if (count > 0)
    {
        // 判断数据是否被全部发送出去了
        if (bufferReadableSize(conn->writeBuf) == 0)
        {
            // 1. 不再检测写事件 -- 修改channel中保存的事件
            writeEventEnable(conn->channel, false);
            // 2. 修改dispatcher检测的集合 -- 添加任务节点
            eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
            // 3. 删除这个节点
            eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
        }
    }
    return 0;
}

struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop* evloop)
{
    struct TcpConnection* conn = (struct TcpConnection*)malloc(sizeof(struct TcpConnection));
    conn->evLoop = evloop;
    conn->readBuf = bufferInit(10240);
    conn->writeBuf = bufferInit(10240);
    // http
    conn->request = httpRequestInit();
    conn->response = httpResponseInit();
    sprintf(conn->name, "Connection-%d", fd);
    conn->channel = channelInit(fd, ReadEvent, processRead, processWrite, tcpConnectionDestroy, conn);
    eventLoopAddTask(evloop, conn->channel, ADD);
    Debug("和客户端建立连接, fd: %ld ,threadName: %s, threadID: %ld, connName: %s",
        fd, evloop->threadName, evloop->threadID, conn->name);

    return conn;
}

// 每个cfd对应一个TcpConnection，因此当cfd断开连接，该TcpConnection就对应释放
// 此资源释放函数，对TcpConnection结构体相关变量除evloop外全部释放
// evloop并不是一个TcpConnection独有，仅是TcpConnection从线程拷贝获得
int tcpConnectionDestroy(void* arg)
{
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    if (conn != NULL)
    {
        if (conn->readBuf && bufferReadableSize(conn->readBuf) == 0 &&
            conn->writeBuf && bufferReadableSize(conn->writeBuf) == 0)
        {
            destroyChannel(conn->evLoop, conn->channel);
            bufferDestroy(conn->readBuf);
            bufferDestroy(conn->writeBuf);
            httpRequestDestroy(conn->request);
            httpResponseDestroy(conn->response);
            free(conn);
        }
    }
    Debug("连接断开, 释放资源, gameover, connName: %s", conn->name);
    return 0;
}
