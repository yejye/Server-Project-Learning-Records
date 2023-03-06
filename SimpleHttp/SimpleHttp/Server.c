#include "Server.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/signal.h>


struct FdInfo
{
	int fd;
	int epfd;
	pthread_t tid;
};

int initListenFd(unsigned short port)
{
	// 1.�����׽���
	int lfd = socket(AF_INET, SOCK_STREAM, 0); //ipv4 ��ʽ tcp
	if (lfd == -1) 
	{
		perror("socket\n");
		return -1;
	}
	// 2.���ö˿ڸ��ã������˳����˿�δ���ͷ�ʱ���˿ڿ�ֱ�ӱ��µķ��������̰󶨣�
	int opt = 1;
	int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
	if (ret == -1)
	{
		perror("setsockopt\n");
		return -1;
	}
	// 3.��
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	ret = bind(lfd, (struct sockaddr*)&addr, sizeof addr);
	if (ret == -1)
	{
		perror("bind\n");
		return -1;
	}
	// 4.����
	ret = listen(lfd, 128); // ���128
	if (ret == -1)
	{
		perror("listen\n");
		return -1;
	}
	// ���ؼ����׽���
	return lfd;
}

int epollRun(int lfd)
{
	// 1.����epollʵ����һ�ù�������
	int epfd = epoll_create(1);
	if (epfd == -1)
	{
		perror("epoll create\n");
		return -1;
	}
	// 2.��lfd����epfd
	struct epoll_event ev;
	ev.data.fd = lfd;
	ev.events = EPOLLIN; // �������¼�
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if (ret == -1)
	{
		perror("epoll ctl\n");
		return -1;
	}
	// 3.ѭ����⣺����epoll��io��·����
	// ����һ�������¼Ҫ������¼�����
	struct epoll_event evs[1024];
	int size = sizeof(evs) / sizeof(struct epoll_event);
	while (1) 
	{
		int num = epoll_wait(epfd, evs, size, -1); // -1��������
		for (int i = 0; i < num; i++) 
		{
			struct FdInfo* info = (struct FdInfo*)malloc(sizeof(struct FdInfo));
			int fd = evs[i].data.fd;
			info->epfd = epfd;
			info->fd = fd;
			if (fd == lfd) {
				// �Լ����׽�����Ҫ�������Ӳ���
				// ���������� accept
				// acceptClient(lfd, epfd); // ���߳�
				pthread_create(&info->tid, NULL, acceptClient, info);
			}
			else {
				// ͨ���׽�����Ҫ���ж�����
				//recvHttpRequest(fd, epfd);
				pthread_create(&info->tid, NULL, recvHttpRequest, info);
			}
		}
	}
	return 0;
}

void* acceptClient(void* arg)
{
	struct FdInfo* info = (struct FdInfo*)arg;
	int epfd = info->epfd;
	int lfd = info->fd;
	// 1.�������ӣ�����ͨ���׽���
	int cfd = accept(lfd, NULL, NULL);
	if (cfd == -1) 
	{
		perror("accept\n");
		return NULL;
	}
	// 2.����cfd����������epoll����Ϊ����ģʽ����Ҫ���÷�����
	int flag = fcntl(cfd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(cfd, F_SETFL, flag);
	// 3.��cfd����epoll����
	struct epoll_event ev;
	ev.data.fd = cfd;
	ev.events = EPOLLIN | EPOLLET; // ���ü����¼��ұ���ģʽ
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
	if (ret == -1)
	{
		perror("epoll ctl\n");
		return NULL;
	}
	printf("acceptcliet threadId: %ld\n", info->tid);
	free(info);
	return NULL;
}

//���ڲ��ñ���ģʽ����Ҫһ���Զ���
void* recvHttpRequest(void* arg)
{
	printf("��ʼ����������...\n");
	struct FdInfo* info = (struct FdInfo*)arg;
	int epfd = info->epfd;
	int cfd = info->fd;
	char buf[4096]; // ���get�������ݽ϶�
	char tmp[1024];
	int len = 0, total = 0;
	while ((len = recv(cfd, tmp, sizeof tmp, 0)) > 0)
	{
		if (len + total < sizeof buf) 
		{
			memcpy(buf + total, tmp, len);
		}
		total += len;
	}
	
	// �ж��Ƿ��ȡ����
	if (len == -1 && errno == EAGAIN)
	{
		// ��ȡ����������http������
		char* pt = strstr(buf, "\r\n"); // �����Ӵ���һ�γ��ֵ�ָ��
		int reqLen = pt - buf;
		buf[reqLen] = '\0';
		parseRequestLine(buf, cfd); // �Է����״̬�н��н���
	}
	else if (len == 0) // ��ʱ�ͻ�����������Ͽ�����
	{
		epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
		close(cfd);
	}
	else // ��ȡʧ��
	{
		perror("recv\n");
		//return NULL;
	}
	printf("recvMsg threadId: %ld\n", info->tid);
	free(info);
	return NULL;
}

int parseRequestLine(const char* line, int cfd)
{
	// ���������� get /xxx/1.jpg http/1.1
	char method[12];
	char path[1024];
	// ��ʽ���ַ�����ȡָ���ַ���
	sscanf(line, "%[^ ] %[^ ]", method, path);
	printf("method: %s, path: %s\n", method, path);
	// �ж�http���󷽷���Ŀǰ������get
	if (strcasecmp(method, "get") != 0) {
		return -1; // �����ִ�Сд�ıȽ�
	}
	// ��·���е������ַ���ԭ
	decodeMsg(path, path);
	// ����������Դ��ַ���ļ���Ŀ¼��
	char* file = NULL;
	if (strcmp(path, "/") == 0) {
		file = "./";
	}
	else
	{
		file = path + 1;
	}
	// ��ȡ��ǰ�ļ����ԣ��ж����ļ�����Ŀ¼��
	struct stat st;
	int ret = stat(file, &st);
	if (ret == -1) 
	{
		// �ļ������ڣ��ظ�404
		sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
		sendFile("404.html", cfd);
		return 0;
	}
	if (S_ISDIR(st.st_mode)) 
	{
		// ��Ŀ¼������Ŀ¼����
		sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
		sendDir(file, cfd);
	}
	else
	{
		// ���ļ��������ļ�����
		// ���Ͱ��������֣���Ӧͷ+�ļ�����
		sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
		sendFile(file, cfd);
	}
	return 0;
}

const char* getFileType(const char* name)
{
	// a.jpg a.mp4 a.html
	// ����������ҡ�.���ַ�, �粻���ڷ���NULL
	const char* dot = strrchr(name, '.');
	if (dot == NULL)
		return "text/plain; charset=utf-8";	// ���ı�
	if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
		return "text/html; charset=utf-8";
	if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
		return "image/jpeg";
	if (strcmp(dot, ".gif") == 0)
		return "image/gif";
	if (strcmp(dot, ".png") == 0)
		return "image/png";
	if (strcmp(dot, ".css") == 0)
		return "text/css";
	if (strcmp(dot, ".au") == 0)
		return "audio/basic";
	if (strcmp(dot, ".wav") == 0)
		return "audio/wav";
	if (strcmp(dot, ".avi") == 0)
		return "video/x-msvideo";
	if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
		return "video/quicktime";
	if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
		return "video/mpeg";
	if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
		return "model/vrml";
	if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
		return "audio/midi";
	if (strcmp(dot, ".mp3") == 0)
		return "audio/mpeg";
	if (strcmp(dot, ".ogg") == 0)
		return "application/ogg";
	if (strcmp(dot, ".pac") == 0)
		return "application/x-ns-proxy-autoconfig";

	return "text/plain; charset=utf-8";
}


/* html��ʽʾ�⣬����Ŀ¼����html
<html>
	<head>
		<title>test</title>
	</head>
	<body>
		<table>
			<tr>
				<td></td>
				<td></td>
			</tr>
			<tr>
				<td></td>
				<td></td>
			</tr>
		</table>
	</body>
</html>
*/

int sendDir(const char* dirName, int cfd)
{
	char buf[4096] = { 0 };
	// htmlͷ�����
	sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName);
	// ��ȡĿ¼����
	struct dirent** namelist;
	int num = scandir(dirName, &namelist, NULL, alphasort);
	for (int i = 0; i < num; i++) 
	{
		// ��ǰȡ����Ŀ¼�����Ǵ�һ�����·�����貹ȫ
		char* name = namelist[i]->d_name;
		char subPath[1024] = { 0 };
		struct stat st;
		sprintf(subPath, "%s/%s", dirName, name);
		stat(subPath, &st);
		if (S_ISDIR(st.st_mode))
		{
			// ��Ŀ¼
			sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
				name, name, st.st_size);
			// <a href="">���·����ת��Ŀ¼��ǰ��'/'�����'/'
			// �൱�ڷ������µ�����
		}
		else
		{
			// ���ļ�
			sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
				name, name, st.st_size);
		}
		// �ߴ�߷�
		send(cfd, buf, strlen(buf), 0);
		memset(buf, 0, sizeof(buf));
		free(namelist[i]); // scandir�����ж�̬�����˵�ַ
	}
	// ����html������β
	sprintf(buf, "</table></body></html>");
	send(cfd, buf, strlen(buf), 0);
	free(namelist);
	return 0;
}

int sendFile(const char* fileName, int cfd)
{
	// 1.���ļ�
	int fd = open(fileName, O_RDONLY);
	assert(fd > 0); // ���ԣ��򻯴��ж�
	//int total=0;

#if 0
	// ����ѡ���ȴ��ļ��������ٷ��͵��ͻ���
	while (1) // �ļ��ϴ�ѭ������
	{
		char buf[1024];
		int len = read(fd, buf, sizeof buf);
		if (len > 0)
		{
			//total += len;
			send(cfd, &buf, len, MSG_NOSIGNAL); // MSG_NOSIGNAL �������ú���SIGPIPE�ź�
			usleep(10); // ���ⷢ��̫�죬�ͻ���������������ɴ���
		}
		else if (len == 0)
		{
			// �������
			//printf("total=   %ld", total);
			break;
		}
		else
		{
			perror("read\n");
			// �˴�û��ֱ�� return -1 
		}
	}
#else
	//����ϵͳ����ֱ�ӷ����ļ�
	off_t offset = 0;
	int size = lseek(fd, 0, SEEK_END); // �ļ���С
	lseek(fd, 0, SEEK_SET); // ������������ڿ�ͷ
	//struct stat st;
	//stat(fileName, &st);
	//int size = st.st_size;
	while (offset < size) 
	{ 
		// �ļ�̫��ѭ������
		int ret = sendfile(cfd, fd, &offset, size - offset);
		printf("ret value: %d\n", ret);
		// ����offsetʼ�մﲻ��size������������������������������������������ܵ��������źŴ��
		// printf("%ld      %ld", offset, size);
		if (ret == -1 && errno == EAGAIN)
		{
			// cfdΪ������������̫����
			printf("wait...\n");
		}
		usleep(10);
	}
#endif
	close(fd);
	return 0;
}


int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length)
{
	// ״̬�� http/1.1 ״̬�� ״̬
	char buf[4096] = { 0 }; ///// ��Ϊû��ֵ����404ҳ�淵�ز���
	sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
	// ��Ӧ��
	sprintf(buf + strlen(buf), "content-type: %s\r\n", type);
	sprintf(buf + strlen(buf), "content-length: %d\r\n\r\n", length); ;

	send(cfd, buf, strlen(buf), 0);
	return 0;
}

// ���ַ�ת��Ϊ������
int hexToDec(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0;
}

// ����
// to �洢����֮�������, ��������, from�����������, �������
void decodeMsg(char* to, char* from)
{
	for (; *from != '\0'; ++to, ++from)
	{
		// isxdigit -> �ж��ַ��ǲ���16���Ƹ�ʽ, ȡֵ�� 0-f
		// Linux%E5%86%85%E6%A0%B8.jpg
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
		{
			// ��16���Ƶ��� -> ʮ���� �������ֵ��ֵ�����ַ� int -> char
			// B2 == 178
			// ��3���ַ�, �����һ���ַ�, ����ַ�����ԭʼ����
			*to = hexToDec(from[1]) * 16 + hexToDec(from[2]);

			// ���� from[1] �� from[2] ����ڵ�ǰѭ�����Ѿ��������
			from += 2;
		}
		else
		{
			// �ַ�����, ��ֵ
			*to = *from;
		}

	}
	*to = '\0';
}


void SetupSignal()
{
	struct sigaction sa;
	sa.sa_handler= SIG_IGN;
	sa.sa_flags = 0;
	if (sigaction(SIGPIPE, &sa, NULL) == 0)
	{
		printf("SIGPIPE ignore");
	}
}
