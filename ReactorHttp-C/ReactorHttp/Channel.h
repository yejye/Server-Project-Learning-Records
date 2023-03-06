#pragma once
#include <stdbool.h>

// Channel模块（通信的管道）
// 主要是对fd相关信息的封装

// 定义一个函数指针类型:handleFunc
typedef int(*handleFunc)(void* arg);

// 定义文件描述符的读写事件
enum FDEvent
{
    TimeOut = 0x01,
    ReadEvent = 0x02,
    WriteEvent = 0x04
};

struct Channel
{
    // 文件描述符
    int fd;
    // 事件
    int events;
    // 回调函数
    handleFunc readCallback;
    handleFunc writeCallback;
    handleFunc destroyCallback;
    // 回调函数的参数
    void* arg;
};

// 初始化一个Channel
// 有三种类型Channel：1.用于监听的lfd 2.用于本地通信的socketpair 3.通信cfd 
struct Channel* channelInit(int fd, int events, handleFunc readFunc, handleFunc writeFunc, handleFunc destroyFunc, void* arg);
// 修改fd的写事件(检测 or 不检测)
void writeEventEnable(struct Channel* channel, bool flag);
// 判断是否需要检测文件描述符的写事件
bool isWriteEventEnable(struct Channel* channel);