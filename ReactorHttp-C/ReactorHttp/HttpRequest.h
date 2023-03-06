#pragma once
#include "Buffer.h"
#include <stdbool.h>
#include "HttpResponse.h"

// 请求头键值对
struct RequestHeader
{
    char* key;
    char* value;
};

// 当前的解析状态
enum HttpRequestState
{
    ParseReqLine, // 正在处理请求行
    ParseReqHeaders, // 请求头
    ParseReqBody, // 数据体
    ParseReqDone // 全部完成 
};

// HttpRequest模块是为了解析并处理http报文实现的
struct HttpRequest
{
    // 指针分别保存http请求的关键信息
    // 请求行：请求方式、请求内容、http版本
    char* method;
    char* url;
    char* version;
    // 请求头：包含多组键值对，在POST请求中有用
    struct RequestHeader* reqHeaders;
    int reqHeadersNum;
    // 标志位：标记当前解析状态
    enum HttpRequestState curState;
};

// 初始化
struct HttpRequest* httpRequestInit();
// 重置，简化代码
void httpRequestReset(struct HttpRequest* req);
void httpRequestResetEx(struct HttpRequest* req);
// 销毁，释放资源
void httpRequestDestroy(struct HttpRequest* req);

// 获取当前处理状态
enum HttpRequestState httpRequestState(struct HttpRequest* request);
// 向请求头添加键值对
void httpRequestAddHeader(struct HttpRequest* request, const char* key, const char* value);
// 根据key得到请求头的value
char* httpRequestGetHeader(struct HttpRequest* request, const char* key);

// 解析请求行（从buffer中读取请求行内容，解析存储）
bool parseHttpRequestLine(struct HttpRequest* request, struct Buffer* readBuf);
// 解析请求头（从buffer中读取请求头内容，解析存储）
bool parseHttpRequestHeader(struct HttpRequest* request, struct Buffer* readBuf);
// 解析http请求协议：调用函数对请求进行整体解析流程
bool parseHttpRequest(struct HttpRequest* request, struct Buffer* readBuf,
    struct HttpResponse* response, struct Buffer* sendBuf, int socket);
// 处理http请求协议：构建响应处理
bool processHttpRequest(struct HttpRequest* request, struct HttpResponse* response);

// 解码字符串
void decodeMsg(char* to, char* from);
const char* getFileType(const char* name);
void sendDir(const char* dirName, struct Buffer* sendBuf, int cfd);
void sendFile(const char* fileName, struct Buffer* sendBuf, int cfd);