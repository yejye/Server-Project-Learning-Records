#pragma once
#include "EventLoop.h"
#include <stdbool.h>
#include "WorkerThread.h"

// 定义线程池
struct ThreadPool
{
    // 主程序的反应堆模型：当线程池中没有子线程时，变为单反应堆模式
    struct EventLoop* mainLoop;
    bool isStart;
    int threadNum; // 最大子线程数
    struct WorkerThread* workerThreads; // 子线程组
    int index; // 标记下一个任务分配给index号子线程
};

// 初始化线程池
struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int count);
// 启动线程池
void threadPoolRun(struct ThreadPool* pool);
// 从线程池中取出index子线程的反应堆实例
// 将新任务（建立的新连接）交给子线程
struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool);