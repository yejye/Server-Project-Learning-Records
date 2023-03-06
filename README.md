# Server-Project-Learning-Records
学习用服务器项目，多Reactor多线程服务器

### SimpleHttp       c实现单反应堆多线程简易Http服务器
1. Epoll边缘触发(ET)的IO复用模型，非阻塞IO。
2. 基于正则匹配解析HTTP GET请求报文。

### ReactorHttp-C    c实现多反应堆多线程Http服务器
1. 构建Dispatcher模块，支持Epoll (ET) / Poll / Select三种IO复用模型，EventLoop模块管理任务处理流程。
2. 支持单Reactor / 多Reactor模式切换，多Reactor模式基于主从Reactor模型，解决惊群问题。
3. 线程池管理子线程资源，以Round Robin方式选取子线程分配任务，实现服务器端高并发。
4. 基于行扫描解析HTTP GET请求报文，实现静态资源请求的处理。

### ReactorHttp-Cpp  c++重写多反应堆多线程Http服务器
1. c++改写，基于面向对象思想重整程序逻辑。
2. 智能指针帮助内存管理，STL与c++11新特性简化程序，加强程序可读性。
