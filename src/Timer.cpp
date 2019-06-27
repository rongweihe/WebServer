/********************************************************
*@file   Timer.cpp
*@Author rongweihe
*@Data   2019/03/31
*********************************************************/
#include <sys/time.h>
#include <unistd.h>


#include "Timer.h"
#include "Epoll.h"

size_t TimerNode::m_current_msec = 0;  // 当前时间
const size_t TimerManager::kDefaultTimeOut = 20*1000; // 20s

//超时时间=当前时间+当前超时时间
TimerNode::TimerNode(std::shared_ptr<HttpData> httpData, size_t timeout) : deleted_(false), httpData_(httpData)
{
    current_time();
    expiredTime_ = m_current_msec + timeout;
}
//疑问?
//析构关闭资源的时候，同时析构httpDataMap中的引用
TimerNode::~TimerNode()
{
    if(m_http_data)
    {
        auto it = Epoll::http_data_map_.find(m_http_data->clientSocket_->fd);
        if(it != Epoll::http_data_map_.end())
        {
            Epoll::http_data_map_.erase(it);
        }
    }
}
//gettimeofday函数,它可以返回自1970-01-01 00:00:00到现在经历的秒数
void inline TimerNode::current_time()
{
    struct timeval cur;
    gettimeofday(&cur,NULL);
    m_current_msec = (cur.tv_sec*1000) + (cur.tv_usec/1000);
}

//疑问？
void TimerNode::deleted()
{
    // 删除采用标记删除，并及时析构 HttpData ，关闭描述符
    // 关闭定时器时 httpDataMap 里的HttpData 一起erase
    m_http_data.reset();
    m_deleted = true;
}

//疑问
void TimerManager::add_timer(std::shared_ptr<HttpData> httpData,size_t timeout)
{
    m_shared_timer_node mSTimerNode(new TimerNode(httpData,timeout));
    {
        MutexLockGuard guard(m_lock);
        m_timer_priority_queue.push(mSTimerNode);
        httpData->setTimer(mSTimerNode);
        // 将TimerNode和HttpData关联起来
    }
}
// 处理超时事件
void TimerManager::handle_expired_event()
{
    MutexLockGuard guard(lock_);
    // 更新当前时间
    std::cout << "开始处理超时事件" << std::endl;
    TimerNode::current_time();
    while(!m_timer_priority_queue.empty())
    {
        m_shared_timer_node m_node = m_timer_priority_queue.top();
        if(m_node->isDeleted())
        {
            // 删除节点
            m_timer_priority_queue.pop();
        }
        else if (m_node->isExpire())
        {
            // 过期 删除
            m_timer_priority_queue.pop();
        }
        else
        {
            break;
        }
    }
}
