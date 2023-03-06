#pragma once

// ��ʼ������socket
int initListenFd(unsigned short port);

// ����epoll��epoll��һ�ù���io��·���õ�����
int epollRun(int lfd);

// ��ͻ��˽������ӣ�ͬʱ��ͨ���׽��ֽ���epoll����
// void* acceptClient(int lfd, int epfd);
void* acceptClient(void* arg);

// ����http����
// void* recvHttpRequest(int fd, int epfd);
void* recvHttpRequest(void* arg);

// ���������У����������Ӧ
int parseRequestLine(const char* line, int cfd);

// �����ļ�����
int sendFile(const char* fileName, int cfd);
// ������Ӧͷ��״̬��+��Ӧͷ������������״̬���״̬
int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length);
const char* getFileType(const char* name); // ���ļ�������ļ�����

// ����Ŀ¼
int sendDir(const char* dirName, int cfd);

// ת������·��
int hexToDec(char c);
void decodeMsg(char* to, char* from);

// ����SIGPIPE�źţ����ܳ��ֹܵ����������
void SetupSignal();