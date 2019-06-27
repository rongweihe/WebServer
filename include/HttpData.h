/********************************************************
*@file  封装HttpData类
*@Author rongweihe
*@Data 2019/04/18
*********************************************************/
#ifndef HTTPDATA_H_INCLUDED
#define HTTPDATA_H_INCLUDED

#include "HttpParse.h"
#include "HttpResponse.h"
#include "Socket.h"
#include "Timer.h"
#include <memory>

class TimerNode;

// C++11 新特性之十：enable_shared_from_this
// std::enable_shared_from_this 能让一个对象（假设其名为 t ，且已被一个 std::shared_ptr 对象 pt 管理）安全地生成其他额外的
// std::shared_ptr 实例（假设名为 pt1, pt2, ... ） ，它们与 pt 共享对象 t 的所有权。
class HttpData : public std::enable_shared_from_this<HttpData> {
public:
    HttpData() : epoll_fd(-1) {}

public:
    std::shared_ptr<HttpRequest> request_;
    std::shared_ptr<HttpResponse> response_;
    std::shared_ptr<ClientSocket> clientSocket_;
    int epoll_fd;
    void closeTime();
    void setTimer(std::shared_ptr<TimerNode>);

private:
    std::weak_ptr<TimerNode> weak_ptr_timer_;
};
#endif // HTTPDATA_H_INCLUDED
