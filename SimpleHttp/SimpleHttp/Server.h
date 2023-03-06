#pragma once

// 初始化监听socket
int initListenFd(unsigned short port);

// 启动epoll（epoll是一棵管理io多路复用的树）
int epollRun(int lfd);

// 与客户端建立连接，同时将通信套接字交给epoll管理
// void* acceptClient(int lfd, int epfd);
void* acceptClient(void* arg);

// 接收http请求
// void* recvHttpRequest(int fd, int epfd);
void* recvHttpRequest(void* arg);

// 解析请求行，并分情况响应
int parseRequestLine(const char* line, int cfd);

// 发送文件内容
int sendFile(const char* fileName, int cfd);
// 发送响应头（状态行+响应头）：参数包含状态码和状态
int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length);
const char* getFileType(const char* name); // 从文件名获得文件类型

// 发送目录
int sendDir(const char* dirName, int cfd);

// 转换中文路径
int hexToDec(char c);
void decodeMsg(char* to, char* from);

// 忽略SIGPIPE信号（可能出现管道崩溃情况）
void SetupSignal();