#include "ThreadPool.h"
#include <assert.h>
#include <stdlib.h>

struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int count)
{
    struct ThreadPool* pool = (struct ThreadPool*)malloc(sizeof(struct ThreadPool));
    pool->index = 0;
    pool->isStart = false;
    pool->mainLoop = mainLoop;
    pool->threadNum = count;
    pool->workerThreads = (struct WorkerThread*)malloc(sizeof(struct WorkerThread) * count);
    return pool;
}

void threadPoolRun(struct ThreadPool* pool)
{
    assert(pool && !pool->isStart);
    // 对当前线程进行验证：仅主线程可以启动线程池
    if (pool->mainLoop->threadID != pthread_self())
    {
        exit(0);
    }
    pool->isStart = true;
    // 若线程池子线程不为0，初始化子线程并启动
    // 如果子线程数量为0，则为单反应堆模式
    if (pool->threadNum)
    {
        for (int i = 0; i < pool->threadNum; ++i)
        {
            workerThreadInit(&pool->workerThreads[i], i);
            workerThreadRun(&pool->workerThreads[i]);
        }
    }
}

struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool)
{
    assert(pool && pool->isStart);
    if (pool->mainLoop->threadID != pthread_self())
    {
        exit(0);
    }
    // 从线程池中找一个子线程, 然后取出里边的反应堆实例
    // 如果线程池里没有子线程，取出主线程
    struct EventLoop* evLoop = pool->mainLoop;
    if (pool->threadNum > 0)
    {
        evLoop = pool->workerThreads[pool->index].evLoop;
        // index只有主线程操作，不存在线程同步问题
        pool->index = ++pool->index % pool->threadNum;
    }
    return evLoop;
}
