#include "TcpServer.h"
#include <arpa/inet.h>
#include "TcpConnection.h"
#include <stdio.h>
#include <stdlib.h>
#include "Log.h"

struct TcpServer* tcpServerInit(unsigned short port, int threadNum)
{
    struct TcpServer* tcp = (struct TcpServer*)malloc(sizeof(struct TcpServer));
    tcp->listener = listenerInit(port);
    tcp->mainLoop = eventLoopInit();
    tcp->threadNum = threadNum;
    tcp->threadPool = threadPoolInit(tcp->mainLoop, threadNum);
    return tcp;
}

struct Listener* listenerInit(unsigned short port)
{
    struct Listener* listener = (struct Listener*)malloc(sizeof(struct Listener));
    // 1. 创建监听的fd
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket");
        return NULL;
    }
    // 2. 设置端口复用
    int opt = 1;
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (ret == -1)
    {
        perror("setsockopt");
        return NULL;
    }
    // 3. 绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd, (struct sockaddr*)&addr, sizeof addr);
    if (ret == -1)
    {
        perror("bind");
        return NULL;
    }
    // 4. 设置监听
    ret = listen(lfd, 128);
    if (ret == -1)
    {
        perror("listen");
        return NULL;
    }
    // 返回fd
    listener->lfd = lfd;
    listener->port = port;
    return listener;
}

// 主线程的读事件回调函数
int acceptConnection(void* arg)
{
    struct TcpServer* server = (struct TcpServer*)arg;
    // 和客户端建立连接
    int cfd = accept(server->listener->lfd, NULL, NULL);

    // 从线程池中取出一个子线程的反应堆实例, 去处理这个cfd
    struct EventLoop* evLoop = takeWorkerEventLoop(server->threadPool);
    // 子线程处理连接，每个TcpConnection管理一个cfd
    // TcpConnection将cfd交给线程对应的evLoop监控处理
    // 初始化的TcpConnection对象作cfd对应channnel中的arg成员
    tcpConnectionInit(cfd, evLoop); 
    return 0;
}

void tcpServerRun(struct TcpServer* server)
{
    Debug("服务器程序已经启动了...");
    // 启动线程池
    threadPoolRun(server->threadPool);
    // 添加检测的任务，启动mainloop
    // 初始化一个channel实例
    struct Channel* channel = channelInit(server->listener->lfd,
        ReadEvent, acceptConnection, NULL, NULL, server);
    eventLoopAddTask(server->mainLoop, channel, ADD);

    eventLoopRun(server->mainLoop);
}
