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
	// 1.创建套接字
	int lfd = socket(AF_INET, SOCK_STREAM, 0); //ipv4 流式 tcp
	if (lfd == -1) 
	{
		perror("socket\n");
		return -1;
	}
	// 2.设置端口复用（程序退出但端口未被释放时，端口可直接被新的服务器进程绑定）
	int opt = 1;
	int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
	if (ret == -1)
	{
		perror("setsockopt\n");
		return -1;
	}
	// 3.绑定
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
	// 4.监听
	ret = listen(lfd, 128); // 最高128
	if (ret == -1)
	{
		perror("listen\n");
		return -1;
	}
	// 返回监听套接字
	return lfd;
}

int epollRun(int lfd)
{
	// 1.创建epoll实例（一棵管理树）
	int epfd = epoll_create(1);
	if (epfd == -1)
	{
		perror("epoll create\n");
		return -1;
	}
	// 2.将lfd加入epfd
	struct epoll_event ev;
	ev.data.fd = lfd;
	ev.events = EPOLLIN; // 监听读事件
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if (ret == -1)
	{
		perror("epoll ctl\n");
		return -1;
	}
	// 3.循环检测：开启epoll的io多路复用
	// 创建一个数组记录要处理的事件队列
	struct epoll_event evs[1024];
	int size = sizeof(evs) / sizeof(struct epoll_event);
	while (1) 
	{
		int num = epoll_wait(epfd, evs, size, -1); // -1代表阻塞
		for (int i = 0; i < num; i++) 
		{
			struct FdInfo* info = (struct FdInfo*)malloc(sizeof(struct FdInfo));
			int fd = evs[i].data.fd;
			info->epfd = epfd;
			info->fd = fd;
			if (fd == lfd) {
				// 对监听套接字需要进行连接操作
				// 建立新连接 accept
				// acceptClient(lfd, epfd); // 单线程
				pthread_create(&info->tid, NULL, acceptClient, info);
			}
			else {
				// 通信套接字需要进行读操作
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
	// 1.建立连接，创建通信套接字
	int cfd = accept(lfd, NULL, NULL);
	if (cfd == -1) 
	{
		perror("accept\n");
		return NULL;
	}
	// 2.设置cfd非阻塞：当epoll设置为边沿模式，需要设置非阻塞
	int flag = fcntl(cfd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(cfd, F_SETFL, flag);
	// 3.将cfd交给epoll管理
	struct epoll_event ev;
	ev.data.fd = cfd;
	ev.events = EPOLLIN | EPOLLET; // 设置检测读事件且边沿模式
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

//由于采用边沿模式，需要一次性读出
void* recvHttpRequest(void* arg)
{
	printf("开始接收数据了...\n");
	struct FdInfo* info = (struct FdInfo*)arg;
	int epfd = info->epfd;
	int cfd = info->fd;
	char buf[4096]; // 获得get请求，内容较短
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
	
	// 判断是否读取结束
	if (len == -1 && errno == EAGAIN)
	{
		// 读取结束，解析http请求行
		char* pt = strstr(buf, "\r\n"); // 查找子串第一次出现的指针
		int reqLen = pt - buf;
		buf[reqLen] = '\0';
		parseRequestLine(buf, cfd); // 对分离的状态行进行解析
	}
	else if (len == 0) // 此时客户端与服务器断开连接
	{
		epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
		close(cfd);
	}
	else // 读取失败
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
	// 解析请求行 get /xxx/1.jpg http/1.1
	char method[12];
	char path[1024];
	// 格式化字符串，取指定字符集
	sscanf(line, "%[^ ] %[^ ]", method, path);
	printf("method: %s, path: %s\n", method, path);
	// 判断http请求方法：目前仅考虑get
	if (strcasecmp(method, "get") != 0) {
		return -1; // 不区分大小写的比较
	}
	// 将路径中的中文字符还原
	decodeMsg(path, path);
	// 处理请求资源地址（文件或目录）
	char* file = NULL;
	if (strcmp(path, "/") == 0) {
		file = "./";
	}
	else
	{
		file = path + 1;
	}
	// 获取当前文件属性（判断是文件还是目录）
	struct stat st;
	int ret = stat(file, &st);
	if (ret == -1) 
	{
		// 文件不存在：回复404
		sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
		sendFile("404.html", cfd);
		return 0;
	}
	if (S_ISDIR(st.st_mode)) 
	{
		// 是目录，发送目录内容
		sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
		sendDir(file, cfd);
	}
	else
	{
		// 是文件，发送文件内容
		// 发送包含两部分：响应头+文件内容
		sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
		sendFile(file, cfd);
	}
	return 0;
}

const char* getFileType(const char* name)
{
	// a.jpg a.mp4 a.html
	// 自右向左查找‘.’字符, 如不存在返回NULL
	const char* dot = strrchr(name, '.');
	if (dot == NULL)
		return "text/plain; charset=utf-8";	// 纯文本
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


/* html格式示意，根据目录构建html
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
	// html头部完成
	sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName);
	// 获取目录内容
	struct dirent** namelist;
	int num = scandir(dirName, &namelist, NULL, alphasort);
	for (int i = 0; i < num; i++) 
	{
		// 当前取出的目录名称是次一级相对路径，需补全
		char* name = namelist[i]->d_name;
		char subPath[1024] = { 0 };
		struct stat st;
		sprintf(subPath, "%s/%s", dirName, name);
		stat(subPath, &st);
		if (S_ISDIR(st.st_mode))
		{
			// 是目录
			sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
				name, name, st.st_size);
			// <a href="">向对路径跳转子目录：前无'/'后面加'/'
			// 相当于发送了新的请求
		}
		else
		{
			// 是文件
			sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
				name, name, st.st_size);
		}
		// 边存边发
		send(cfd, buf, strlen(buf), 0);
		memset(buf, 0, sizeof(buf));
		free(namelist[i]); // scandir函数中动态分配了地址
	}
	// 发送html后续结尾
	sprintf(buf, "</table></body></html>");
	send(cfd, buf, strlen(buf), 0);
	free(namelist);
	return 0;
}

int sendFile(const char* fileName, int cfd)
{
	// 1.打开文件
	int fd = open(fileName, O_RDONLY);
	assert(fd > 0); // 断言，简化打开判断
	//int total=0;

#if 0
	// 可以选择先从文件拷贝，再发送到客户端
	while (1) // 文件较大循环发送
	{
		char buf[1024];
		int len = read(fd, buf, sizeof buf);
		if (len > 0)
		{
			//total += len;
			send(cfd, &buf, len, MSG_NOSIGNAL); // MSG_NOSIGNAL 参数设置忽略SIGPIPE信号
			usleep(10); // 避免发送太快，客户端来不及处理造成错误
		}
		else if (len == 0)
		{
			// 发送完毕
			//printf("total=   %ld", total);
			break;
		}
		else
		{
			perror("read\n");
			// 此处没有直接 return -1 
		}
	}
#else
	//采用系统函数直接发送文件
	off_t offset = 0;
	int size = lseek(fd, 0, SEEK_END); // 文件大小
	lseek(fd, 0, SEEK_SET); // 将光标重新置于开头
	//struct stat st;
	//stat(fileName, &st);
	//int size = st.st_size;
	while (offset < size) 
	{ 
		// 文件太大循环发送
		int ret = sendfile(cfd, fd, &offset, size - offset);
		printf("ret value: %d\n", ret);
		// 出现offset始终达不到size的情况？？？？？？？？？？？——————被管道崩溃的信号打断
		// printf("%ld      %ld", offset, size);
		if (ret == -1 && errno == EAGAIN)
		{
			// cfd为非阻塞，读的太快了
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
	// 状态行 http/1.1 状态码 状态
	char buf[4096] = { 0 }; ///// 因为没赋值导致404页面返回不了
	sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
	// 响应行
	sprintf(buf + strlen(buf), "content-type: %s\r\n", type);
	sprintf(buf + strlen(buf), "content-length: %d\r\n\r\n", length); ;

	send(cfd, buf, strlen(buf), 0);
	return 0;
}

// 将字符转换为整形数
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

// 解码
// to 存储解码之后的数据, 传出参数, from被解码的数据, 传入参数
void decodeMsg(char* to, char* from)
{
	for (; *from != '\0'; ++to, ++from)
	{
		// isxdigit -> 判断字符是不是16进制格式, 取值在 0-f
		// Linux%E5%86%85%E6%A0%B8.jpg
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
		{
			// 将16进制的数 -> 十进制 将这个数值赋值给了字符 int -> char
			// B2 == 178
			// 将3个字符, 变成了一个字符, 这个字符就是原始数据
			*to = hexToDec(from[1]) * 16 + hexToDec(from[2]);

			// 跳过 from[1] 和 from[2] 因此在当前循环中已经处理过了
			from += 2;
		}
		else
		{
			// 字符拷贝, 赋值
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
