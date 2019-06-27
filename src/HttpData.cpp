/********************************************************
*@file  封装HttpData.cpp
*@Author rongweihe
*@Data 2019/03/31
*********************************************************/
#include "HttpData.h"

void HttpData::closeTime()
{
    //首先判断Timer是否还存在，有可能超时释放
    if(weak_ptr_timer_.lock())
    {
        std::shared_ptr<TimerNode> tempTimer(weak_ptr_timer_.lock());
        tempTimer->deleted();
        weak_ptr_timer_.reset();// 断开weak_ptr
    }
}
void HttpData::setTimer(std::shared_ptr<TimerNode> timer)
{
    weak_ptr_timer_  = timer;
}
