#include "WorkerThread.h"
#include <stdio.h>

int workerThreadInit(struct WorkerThread* thread, int index)
{
    thread->evLoop = NULL;
    thread->threadID = 0;
    sprintf(thread->name, "SubThread-%d", index);
    pthread_mutex_init(&thread->mutex, NULL);
    pthread_cond_init(&thread->cond, NULL);
    return 0;
}

// 子线程的回调函数（即子线程执行程序）
void* subThreadRunning(void* arg)
{
    struct WorkerThread* thread = (struct WorkerThread*)arg;
    // 启动反应堆模型
    // 由于evLoop可能被子线程与主线程同时访问，加锁
    pthread_mutex_lock(&thread->mutex);
    thread->evLoop = eventLoopInitEx(thread->name);
    pthread_mutex_unlock(&thread->mutex);
    pthread_cond_signal(&thread->cond);
    eventLoopRun(thread->evLoop);
    return NULL;
}

void workerThreadRun(struct WorkerThread* thread)
{
    // 启动子线程——创建子线程
    pthread_create(&thread->threadID, NULL, subThreadRunning, thread);
    // 此处如果不做处理直接退出，可能出现子线程反应堆没有创建完成
    // 因此条件变量阻塞主线程
    pthread_mutex_lock(&thread->mutex);
    while (thread->evLoop == NULL)
    {
        pthread_cond_wait(&thread->cond, &thread->mutex);
    }
    pthread_mutex_unlock(&thread->mutex);
}
