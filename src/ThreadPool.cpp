/*
  Copyright (c) 2019 rongweihe, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Author: rongweihe (rongweihe1995@gmail.com)
  Data:   2019/03/31
  update: 2020/11/09
*/

#include "ThreadPool.h"
#include <bits/stdc++.h>
#include <pthread.h>
#include <sys/prctl.h>

//列表初始化
ThreadPool::ThreadPool(int thread_s, int max_queue_s) : max_queue_size(max_queue_s), m_thread_size(thread_s),
    m_cond(mutex_), started(0), shutdown_(0)
{
    //开启四个工作线程
    if(thread_s <=0 || thread_s > MAX_THREAD_SIZE)
    {
        m_thread_size = 4;
    }
    if (max_queue_s <= 0 || max_queue_s > MAX_QUEUE_SIZE)
    {
        max_queue_size = MAX_QUEUE_SIZE;
    }
    //分配空间
    threads.resize(m_thread_size);

    //创建线程
    //C API:pthread_create 
    //C++ 11 提供 std::thread
    for(int i=0; i<m_thread_size; ++i)
    {
        if(pthread_create(&threads[i],NULL,worker,this)!=0 )
        {
            std::cout << "ThreadPool init error" << std::endl;
            throw std::exception();
        }
        ++started;
    }
}
ThreadPool::~ThreadPool() {}
/*
function fun 相当于 void *(*function) (void *) 函数指针，回调函数
arg 上面函数的参数
*/

// 将任务添加到任务队列中
bool ThreadPool::append(std::shared_ptr<void> arg, std::function<void(std::shared_ptr<void>)> fun)
{
    if(shutdown_)
    {
        std::cout << "ThreadPool has shutdown" << std::endl;
        return false;
    }
    MutexLockGuard guard(this->m_mutex);
    if (request_queue.size() > max_queue_size)
    {
        std::cout << "ThreadPool too many requests" << max_queue_size <<std::endl;
        return false;
    }
    //入队操作
    ThreadTask threadTask;
    threadTask.arg = arg;
    threadTask.process = fun;
    request_queue.push_back(threadTask);
    // 每次加完任务，发个信号给线程,若没有线程处于等待状态
    // pthread_cond_signal() 唤醒一个等待该条件的线程
    cond.notify();
    return true;
}
void ThreadPool::shutdown(bool graceful)
{
    MutexLockGuard guard(this->m_mutex);
    if(shutdown_)
    {
        std::cout << "has shutdown" << std::endl;
    }
    shutdown_ = graceful ? graceful_mode : immediate_mode;
    cond.notifyAll(); /// 唤醒所有等待该条件的线程

    //pthread_join用来等待一个线程的结束,线程间同步的操作
    /*
    在很多情况下，主线程生成并起动了子线程，如果子线程里要进行大量的耗时的运算，主线程往往将于子线程之前结束，
    但是如果主线程处理完其他的事务后，需要用到子线程的处理结果，也就是主线程需要等待子线程执行完成之后再结束，
    这个时候就要用到pthread_join()方法了。
    即pthread_join()的作用可以这样理解：主线程等待子线程的终止。
    也就是在子线程调用了pthread_join()方法后面的代码，只有等到子线程结束了才能执行。
    */
    for(int i=0; i<m_thread_size; ++i)
    {
        if(pthread_join(threads[i],NULL) !=0 )
        {
            std::cout << "pthread_join error" << std::endl;
        }
    }
}
void *ThreadPool::worker(void *args)
{
    ThreadPool *pool = static_cast<ThreadPool *>(args);
    //退出线程
    if(pool == nullptr)
        return NULL;
    //用prctl给线程命名
    prctl(PR_SET_NAME,"EventLoopThread");
    //执行线程主方法
    pool->run();
    return NULL;
}

void ThreadPool::run()
{
    while(true)
    {
        ThreadTask requestTask;
        {
            MutexLockGuard guard(this->m_mutex);
            // 无任务 且未shutdown 则条件等待, 注意此处应使用while而非if
            while(request_queue.empty() && !shutdown_)
            {
                cond.wait();
            }
            if((shutdown_ == immediate_mode) || (shutdown_ == graceful_mode && request_queue.empty()))
            {
                break;
            }
            // FIFO
            requestTask = request_queue.front();
            request_queue.pop_front();
        }
    }
    requestTask.process(requestTask.arg);
}

