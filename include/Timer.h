/********************************************************
*@file  封装定时器类
*@Author rongweihe
*
*
*********************************************************/
#ifndef TIMER_H_INCLUDED
#define TIMER_H_INCLUDED

#include "HttpData.h"
#include "MutexLock.h"
#include <queue>
#include <deque>
#include <memory>

class HttpData;

class TimerNode {
public:
    TimerNode(std::shared_ptr<HttpData> httpData,size_t timeout);
    ~TimerNode();
public:
    bool is_deleted() const { return m_deleted };
    size_t get_expire_time()  {return m_expired_time;}
    bool is_expire() const {
        //频繁调度系统不好
        return m_expired_time < m_current_msec;
    }
    void deleted();
    std::shared_ptr<HttpData> get_http_data() const {
        return m_http_data;
    }
    static void current_time();
    static size_t m_current_msec;//当前时间

private:
    bool m_deleted;
    size_t m_expired_time;//毫秒
    std::shared_ptr<HttpData> m_http_data;
};

struct TimerCmp {
    bool operator() (std::shared_ptr<TimerNode> &a, std::shared_ptr<TimerNode> &b) const {
        return a->get_expire_time() > b->get_expire_time();
    }
};

class TimerManager{
public:
    typedef std::shared_ptr<TimerNode> m_shared_timer_node;
    void add_timer(std::shared_ptr<HttpData> httpData,size_t timeout);
    void handle_expired_event();
    const static size_t kDefaultTimeOut;
private:
    std::priority_queue<m_shared_timer_node,std::deque<m_shared_timer_node>,TimerCmp> m_timer_priority_queue;
    MutexLock m_lock;
};
#endif // TIMER_H_INCLUDED
