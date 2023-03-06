#pragma once

struct Buffer
{
    // 指向内存的指针
    char* data;
    int capacity;
    int readPos;
    int writePos;
};

// 初始化
struct Buffer* bufferInit(int size);
// 释放空间
void bufferDestroy(struct Buffer* buf);
// 扩容
void bufferExtendRoom(struct Buffer* buffer, int size);
// 得到剩余的可写的内存容量
int bufferWriteableSize(struct Buffer* buffer);
// 得到剩余的可读的内存容量
int bufferReadableSize(struct Buffer* buffer);
// 写内存 
// 1. 对buffer直接写入数据
int bufferAppendData(struct Buffer* buffer, const char* data, int size);
// 2. 对buffer直接写入字符串（可能出现提前遇到'\0'，丢失数据）
int bufferAppendString(struct Buffer* buffer, const char* data);
// 3. 接收套接字数据（将套接字接收的数据写入buffer）
int bufferSocketRead(struct Buffer* buffer, int fd);

// 根据\r\n取出一行, 找到其在数据块中的位置, 返回该位置
char* bufferFindCRLF(struct Buffer* buffer);
//将buffer中数据发送给套接字
int bufferSendData(struct Buffer* buffer, int socket);