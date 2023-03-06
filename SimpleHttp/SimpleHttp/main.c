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
	// ��ö˿ں�
	unsigned short port = atoi(argv[1]);
	// �л����̹���·������ԴĿ¼��
	chdir(argv[2]);
#else
	unsigned short port = 10000;
	chdir("/home/yezi/source");
#endif
	// ���ú���SIGPIPE
	SetupSignal();
	// ��ʼ�����ڼ������׽���
	int lfd = initListenFd(port);
	// ��������������
	epollRun(lfd);
	return 0;
}