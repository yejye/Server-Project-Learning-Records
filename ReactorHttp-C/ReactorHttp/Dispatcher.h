#pragma once
#include "Channel.h"
#include "EventLoop.h"

// 分发器 ：仅是一个结构体模型
// 具体实现包含select、poll、epoll三种模式
// 该结构体仅提供函数指针，实例化时依情况定
// Dispatcher操作对象数据（data）存放在EventLoop中
// 因此此处需要先声明EventLoop结构体

struct EventLoop;
struct Dispatcher
{
    // init -- 初始化epoll, poll 或者 select 需要的数据块
    void* (*init)();
    // 添加
    int (*add)(struct Channel* channel, struct EventLoop* evLoop);
    // 删除
    int (*remove)(struct Channel* channel, struct EventLoop* evLoop);
    // 修改
    int (*modify)(struct Channel* channel, struct EventLoop* evLoop);
    // 事件监测
    int (*dispatch)(struct EventLoop* evLoop, int timeout); // 单位: s
    // 清除数据(关闭fd或者释放内存)
    int (*clear)(struct EventLoop* evLoop);
};