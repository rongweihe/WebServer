/********************************************************
*@file  封装套接字类
*@Author rongweihe
*@Date   2019/03/31
*********************************************************/
#ifndef SOCKET_H_INCLUDED
#define SOCKET_H_INCLUDED

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <memory>

class ClientSocket;
void setReusePort(int fd);

class ServerSocket{
public:
    ServerSocket(int port = 8080,const char *ip = nullptr);
    ~ServerSocket();
    void bind();
    void listen();
    int accept(ClientSocket &) const;
    void close();
public:
    sockaddr_in m_addr;
    int listen_fd;
    int epoll_fd;
    int m_port;
    const char *m_ip;
};

class ClientSocket {
public:
    ClientSocket() {fd = -1;}
    void close();
    ~ClientSocket();
    socklen_t m_len;
    sockaddr_in m_addr;
    int fd;
};
#endif // SOCKET_H_INCLUDED
