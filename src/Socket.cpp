/********************************************************
*@file   Socket.cpp
*@Author rongweihe
*@Data   2019/03/31
*********************************************************/
#include "Socket.h"
#include "Util.h"
#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
void setReusePort(int fd)
{
    // getsockopt / setsockopt 获取/设置与某个套接字关联的选项 setsockopt : 端口复用
    int opt = -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(opt));
}

/*
通俗易懂理解 网络编程四个函数
创建一个 socket 相当于买一部手机
bind            相当于给手机电话卡
listen          相当于等待接听
accept          相当于接听之后
*/


/*
UNIX/Linux 的一个哲学是:所有东西都是文件。
socket也不例外,它就是可读、可写、可控制、可关闭的文件描述符。
socket 系统调用可创建一个 socket:
#include <sys/types.h>
#include <sys/socket.h>
int socket( int domain, int type, int protocol );

domain参数告诉系统使用哪个底层协议族。
对TCP/IP协议族而言,该参数应该设置为PF INET (Protocol Family of Internet,用于IPv4)或PF-INET6 (用于IPv6) ;
对于UNIX本地域协议族而言,该参数应该设置为PF UNIX,关于socket系统调用支持的所有协议族,请读者自己参考其man手册。
type参数指定服务类型。
服务类型主要有SOCK STREAM服务(流服务)和SOCKUGRAM (数据报)服务。
对TCP/IP协议族而言,其值取SOCK STREAM表示传输层使用TCP协议,取SOCK DGRAM表示传输层使用UDP协议。

值得指出的是, 自Linux内核版本2.6.17起, type参数可以接受上述服务类型与下面两个重要的标志相与的值:
SOCK-NONBLOCK和SOCKCLOEXEC,它们分别表示将新创建的socket设为非阻塞的,
以及用 fork 调用创建子进程时在子进程中关闭该socket,在内核版本2.6.17之前的Linux中,文件描述符的这两个属性都需要使用额外的系统调用(比如fcntl)来设置。
protocol 参数是在前两个参数构成的协议集合下,再选择一个具体的协议。
不过这个值通常都是唯一的(前两个参数已经完全决定了它的值)。几乎在所有情况下,我们都应该把它设置为0,表示使用默认协议。
socket 系统调用成功时返回一个 socket 文件描述符,失败则返回 -1 并设置 errno
*/
ServerSocket::ServerSocket(int port, const char *ip) : mPort(m_port), mIp(m_ip)
{
    //创建一个 Ipv4 socket 地址
    bzero(&m_addr,sizeof(m_addr));
    //sin_family = 地址族，AF_INET = TCP/Ipv4 协议族
    m_addr.sin_family = AF_INET;
    m_addr.sin_port   = htons(port);
    if(m_ip != nullptr)
    {
        //将字符串表示的IP地址转换为网络字节序整数表示的IP地址
        ::inet_pton(AF_INET,m_ip,&m_addr.sin_addr);
    }
    else
    {
        m_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
        std::cout << "creat socket error in file <" << __FILE__ << "> "<< "at " << __LINE__ << std::endl;
        exit(0);
    }
}
//------------------------------------------命名 socket
/*
创建 socket 时,我们给它指定了地址族,但是并未指定使用该地址族中的哪个具体 socket 地址。
将一个socket与socket地址绑定称为给socket命名。在服务器程序中,我们通常要命名socket,因为只有命名后客户端才能知道该如何连接它。客户端则通常不需要命名socket,而是采用匿名方式,即使用操作系统自动分配的socket地址。命名socket的系统调用是bind,其定义如下:
#include <sys/types.h>
#include <sys/socket.h>
int bind( int sockfd, const struct sockaddr* my addr, socklen t addrlen );
bind将my addr所指的socket地址分配给未命名的sockfd文件描述符, addrlen参数指出该socket地址的长度,
bind成功时返回0,失败则返回-1并设置errno。
其中两种常见的errno是EACCES和EADDRINUSE,它们的含义分别是:
OEACCES,被绑定的地址是受保护的地址,仅超级用户能够访问。比如普通用户将socket绑定到知名服务端口(端口号为0-1023)上时, bind将返回EACCES错误。
口 EADDRINUSE,被绑定的地址正在使用中。比如将socket绑定到一个处于TIME.WAIT状态的socket地址。
*/
void ServerSocket::bind()
{
    int ret = ::bind(listen_fd, (struct sockaddr*)&m_addr, sizeof(m_addr));
    if (ret == -1)
    {
        std::cout << "bind error in file <" << __FILE__ << "> "<< "at " << __LINE__ << std::endl;
        exit(0);
    }
}
//------------------------------------------ 监听 socket
/*
socket被命名之后,还不能马上接受客户连接,我们需要使用如下系统调用来创建一个监听队列以存放待处理的客户连接:
#include <sys/socket.h>
int listen( int sockfd, int backlog ).
sockfa参数指定被监听的socket, backlog参数提示内核监听队列的最大长度。
监听队列的长度如果超过backlog,服务器将不受理新的客户连接,客户端也将收到ECONNREFUSED错误信息。
在内核版本2.2之前的Linux中, backlog参数是指所有处于半连接状态(SYN RCVD)和完全连接状态(ESTABLISHED)的socket的上限。
但自内核版本2.2之后,它只表示处于完全连接状态的socket的上限,
处于半连接状态的socket的上限则由/proc/sys/net/ipv4/tcp-maxsyn-backlog内核参数定义。backlog参数的典型值是5.
listen成功时返回0,失败则返回-1并设置errno.
*/
void ServerSocket::listen()
{
    int ret = ::listen(listen_fd, 1024);
    if (ret == -1)
    {
        std::cout << "listen error in file <" << __FILE__ << "> "<< "at " << __LINE__ << std::endl;
        exit(0);
    }
}


/*------------------------------------------accept 系统调用
从 listen 监听队列中接受一个连接:
#include <sys/types.h>
#include <sys/socket.h>
int accept( int sockfd, struct sockaddr addr, socklen t *addrlen );
sockfd参数是执行过listen系统调用的监听sockete,
addr参数用来获取被接受连接的远端socket地址,
该socket地址的长度由addrlen参数指出
accept成功时返回一个新的连接socket,该socket唯一地标识了被接受的这个连接,
服务器可通过读写该socket来与被接受连接对应的客户端通信. accept失败时返回-1并设置errno.
*/
//疑惑
void ServerSocket::accept(ClientSocket &clientSocket) const
{
    int clientfd = ::accept(listen_fd, NULL, NULL);
    if (clientfd < 0)
    {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
            return clientfd;
        std::cout << "accept error in file <" << __FILE__ << "> "<< "at " << __LINE__ << std::endl;
        std::cout << "clientfd:" << clientfd << std::endl;
        perror("accpet error");
        //exit(0);
    }
    //std::cout << "accept a client： " << clientfd << std::endl;
    clientSocket.fd = clientfd;
    return clientfd;
}

void ServerSocket::close()
{
    if (listen_fd >= 0)
    {
        ::close(listen_fd);
        //std::cout << "定时器超时关闭, 文件描述符:" << listen_fd << std::endl;
        listen_fd = -1;
    }


}
ServerSocket::~ServerSocket()
{
    close();
}

void ClientSocket::close()
{
    if (fd >= 0)
    {
        //std::cout << "文件描述符关闭: " << fd <<std::endl;
        ::close(fd);
        fd = -1;
    }
}
ClientSocket::~ClientSocket()
{
    close();
}
