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

//�б��ʼ��
ThreadPool::ThreadPool(int thread_s, int max_queue_s) : max_queue_size(max_queue_s), m_thread_size(thread_s),
    m_cond(mutex_), started(0), shutdown_(0)
{
    //�����ĸ������߳�
    if(thread_s <=0 || thread_s > MAX_THREAD_SIZE)
    {
        m_thread_size = 4;
    }
    if (max_queue_s <= 0 || max_queue_s > MAX_QUEUE_SIZE)
    {
        max_queue_size = MAX_QUEUE_SIZE;
    }
    //����ռ�
    threads.resize(m_thread_size);

    //�����߳�
    //C API:pthread_create 
    //C++ 11 �ṩ std::thread
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
function fun �൱�� void *(*function) (void *) ����ָ�룬�ص�����
arg ���溯���Ĳ���
*/

// ��������ӵ����������
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
    //��Ӳ���
    ThreadTask threadTask;
    threadTask.arg = arg;
    threadTask.process = fun;
    request_queue.push_back(threadTask);
    // ÿ�μ������񣬷����źŸ��߳�,��û���̴߳��ڵȴ�״̬
    // pthread_cond_signal() ����һ���ȴ����������߳�
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
    cond.notifyAll(); /// �������еȴ����������߳�

    //pthread_join�����ȴ�һ���̵߳Ľ���,�̼߳�ͬ���Ĳ���
    /*
    �ںܶ�����£����߳����ɲ��������̣߳�������߳���Ҫ���д����ĺ�ʱ�����㣬���߳������������߳�֮ǰ������
    ����������̴߳������������������Ҫ�õ����̵߳Ĵ�������Ҳ�������߳���Ҫ�ȴ����߳�ִ�����֮���ٽ�����
    ���ʱ���Ҫ�õ�pthread_join()�����ˡ�
    ��pthread_join()�����ÿ���������⣺���̵߳ȴ����̵߳���ֹ��
    Ҳ���������̵߳�����pthread_join()��������Ĵ��룬ֻ�еȵ����߳̽����˲���ִ�С�
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
    //�˳��߳�
    if(pool == nullptr)
        return NULL;
    //��prctl���߳�����
    prctl(PR_SET_NAME,"EventLoopThread");
    //ִ���߳�������
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
            // ������ ��δshutdown �������ȴ�, ע��˴�Ӧʹ��while����if
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

