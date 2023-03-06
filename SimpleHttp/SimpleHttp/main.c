#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "Server.h"

int main(int argc, char* argv[]) {
#if 1
	if (argc < 3) 
	{
		printf("./a.out port path \n");
		return -1;
	}
	// 获得端口号
	unsigned short port = atoi(argv[1]);
	// 切换进程工作路径（资源目录）
	chdir(argv[2]);
#else
	unsigned short port = 10000;
	chdir("/home/yezi/source");
#endif
	// 设置忽略SIGPIPE
	SetupSignal();
	// 初始化用于监听的套接字
	int lfd = initListenFd(port);
	// 启动服务器程序
	epollRun(lfd);
	return 0;
}