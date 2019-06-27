/********************************************************
*@file  封装Epoll.h
*@Author rongweihe
*@Data 2019/04/18
*********************************************************/
#ifndef EPOLL_H_INCLUDED
#define EPOLL_H_INCLUDED

#include "HttpData.h"
#include "Socket.h"
#include "Timer.h"

#include <unordered_map>
#include <sys/epoll.h>
#include <algorithm>
#include <iostream>
#include <vector>

class Epoll
{
public:

    static int init(int max_events);

    static int addfd(int epoll_fd, int fd, __uint32_t events, std::shared_ptr<HttpData>);

    static int modfd(int epoll_fd, int fd, __uint32_t events, std::shared_ptr<HttpData>);

    static int delfd(int epoll_fd, int fd, __uint32_t events);

    static std::vector<std::shared_ptr<HttpData>>
            poll(const ServerSocket &serverSocket, int max_event, int timeout);

    static void handleConnection(const ServerSocket &serverSocket);

public:
    static std::unordered_map<int, std::shared_ptr<HttpData>> http_data_map_;//类成员变量下划线结尾
    static const int kMaxEvents;//const 变量为k开头，后跟大写开头单词
    static epoll_event *events_;
    static TimerManager timer_manager_;
    const static __uint32_t kDefaultEvents;
};

#endif // EPOLL_H_INCLUDED
