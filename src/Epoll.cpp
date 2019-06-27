/********************************************************
*@file   Epoll.cpp
*@Author rongweihe
*@Data   2019/03/31
*********************************************************/
#include "Epoll.h"
#include "Util.h"
#include <bits/stdc++.h>
#include <sys/epoll.h>

std::unordered_map<int, std::shared_ptr<HttpData> > Epoll::http_data_map_;
const int Epoll::kMaxEvents = 10000;
epoll_event *Epoll::events_;

/// 可读 | ET模 | 保证一个socket连接在任一时刻只被一个线程处理
const __uint32_t Epoll::kDefaultEvents =  (EPOLLIN | EPOLLET | EPOLLONESHOT);

TimerManager Epoll::timer_manager_;

///初始化 epoll_create 函数创建一个额外的文件描述符，来唯一标识内核中的事件表
int Epoll::init(int max_events)
{
    ///max_events 参数给内核一个提示，告诉它事件表需要多大
    ///返回的文件描述符 epoll_fd 将作用于其它所有 epoll 系统调用的第一个参数
    int epoll_fd = ::epoll_create(max_events);
    if (epoll_fd == -1)
    {
        std::cout << "epoll create error" << std::endl;
        exit(-1);
    }
    events_ = new epoll_event[max_events];
    return epoll_fd;
}

int Epoll::addfd(int epoll_fd, int fd, __uint32_t events, std::shared_ptr<HttpData> httpData)
{
    epoll_event event;
    event.events = (EPOLLIN|EPOLLET);
    event.data.fd = fd;
    /// 增加http_data_map_
    http_data_map_[fd] = httpData;
    /// epoll_ctl 操作文件描述符，操作类型有以下三种
    /// EPOLL_CTL_ADD 往事件表中注册 fd 上的事件
    /// EPOLL_CTL_MOD 修改 fd 上的注册事件
    /// EPOLL_CTL_DEL 删除 fd 上的注册事件
    int ret = ::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    if (ret < 0)
    {
        std::cout << "epoll add error" << endl;
        /// 释放httpData
        http_data_map_[fd].reset();
        return -1;
    }
    return 0;
}
///setsockopt : 端口复用
int Epoll::modfd(int epoll_fd, int fd, __uint32_t events, std::shared_ptr<HttpData> httpData)
{
    epoll_event event;
    event.events = events;
    event.data.fd = fd;
    /// 每次更改的时候也更新 http_data_map_
    http_data_map_[fd] = httpData;
    int ret = ::epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
    if (ret < 0)
    {
        std::cout << "epoll mod error" << endl;
        /// 释放httpData
        http_data_map_[fd].reset();
        return -1;
    }
    return 0;
}

int Epoll::delfd(int epoll_fd, int fd, __uint32_t events)
{
    epoll_event event;
    event.events = events;
    event.data.fd = fd;
    int ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event);
    if (ret < 0)
    {
        std::cout << "epoll del error" << endl;
        return -1;
    }
    auto it = http_data_map_.find(fd);
    if (it != http_data_map_.end())
    {
        http_data_map_.erase(it);
    }
    return 0;
}
///疑惑
int Epoll::handleConnection(const ServerSocket &serverSocket)
{
    std::shared_ptr<ClientSocket> tempClient(new ClientSocket);
    /// epoll 是 ET 模式，循环接收连接
    /// 需要将 listen_fd 设置为 non-blocking
    while(serverSocket.accept(*tempClient) > 0)
    {
        /// 设置非阻塞
        int ret = setnonblocking(tempClient->fd);
        if (ret < 0)
        {
            std::cout << "setnonblocking error" << std::endl;
            tempClient->close();
            continue;
        }
        std::shared_ptr<HttpData> m_shared_http_data(new HttpData);
        m_shared_http_data->request_  = std::shared_ptr<HttpRequest>(new HttpRequest());
        m_shared_http_data->response_ = std::shared_ptr<HttpRequest>(new HttpResponse());

        std::shared_ptr<ClientSocket> m_shared_client_socket(new ClientSocket());

        //???
        m_shared_client_socket.swap(tempClient);
        m_shared_http_data->clientSocket_ = m_shared_client_socket;
        m_shared_http_data->epoll_fd = serverSocket.epoll_fd;

        addfd(serverSocket.epoll_fd,m_shared_client_socket->fd,kDefaultEvents,m_shared_http_data);
    }
}

// 返回活跃事件数 + 分发处理函数
std::vector<std::shared_ptr<HttpData>> Epoll::poll(const ServerSocket &serverSocket, int max_event, int timeout)
{
    // epoll_wait 函数是 epoll 系统调用的主要接口，它在一段超时时间内等待一组文件描述符上的事件。
    // 成功返回就绪文件描述符个数，失败返回 -1 并设置 errno
    int event_num = epoll_wait(serverSocket.epoll_fd,events_,max_event,timeout);
    if( event_num < 0 )
    {
        std::cout << "epoll_num=" << event_num << std::endl;
        std::cout << "epoll_wait error" << std::endl;
        std::cout << errno << std::endl;
        exit(-1);
    }
    //遍历 events 集合
    std::vector<std::shared_ptr<HttpData>> httpDatas;
    for(int i=0; i<event_num; ++i)
    {
        int fd = events_[i].data.fd;

        //监听描述符
        if(fd == serverSocket.listen_fd)
        {
            handleConnection(serverSocket);
        }
        else
        {
            //出错的描述符，移除定时器，关闭文件描述符
            if ((events_[i].events & EPOLLERR) || (events_[i].events & EPOLLRDHUP) || (events_[i].events & EPOLLHUP))
            {
                auto it = http_data_map_.find(fd);
                if( it != http_data_map_.end())
                {
                    //将HttpData节点和TimerNode的关联分开，这样HttpData会立即析构，在析构函数内关闭文件描述符等资源
                    it->second->closeTime();
                }
                continue;
            }
            //疑问
            auto it = http_data_map_.find(fd);
            if (it != http_data_map_.end())
            {
                if ((events_[i].events & EPOLLIN) || (events_[i].events & EPOLLPRI))
                {
                    http_data_map_.push_back(it->second);
                    std::cout << "定时器中找到:" << fd << std::endl;
                    // 清除定时器 HttpData.closeTimer()
                    it->second->closeTime();
                    http_data_map_.erase(it);
                }
            }
            else
            {
                std::cout << "长连接第二次连接未找到" << std::endl;
                ::close(fd);
                continue;
            }
        }
    }
    return httpDatas;
}


/*
void epoll_run(int port)
{
    // 创建一个 epoll 树的根节点
    int epoll_fd = epoll_create(MAX_THREAD_SIZE);
    if(epoll_fd == -1)
    {
        perror("epoll_create error");
        exit(1);
    }
    // 添加到要监听的节点
    // 委托内核检测添加到树上的节点
    struct epoll_event all[MAX_THREAD_SIZE];
    all[0].events
    while(1)
    {
        int ret = epoll_wait(epoll_fd,all,MAX_THREAD_SIZE,-1);
        if(ret == -1 )
        {
            perror("epoll_wait error");
            exit(1);
        }
        //遍历发生变化的节点
        for(int i=0; i<ret; ++i)
        {

        }
    }
}
*/
