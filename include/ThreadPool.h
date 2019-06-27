#ifndef THREADPOOL_H_INCLUDED
#define THREADPOOL_H_INCLUDED


#include <bits/stdc++.h>
#include <functional>
#include <pthread.h>
#include <memory>
#include "MutexLock.h"
#include "cond.h"

const int MAX_THREAD_SIZE = 1024;
const int MAX_QUEUE_SIZE = 10000;


/*优雅关闭连接*/
typedef enum {
    immediate_mode = 1,
    graceful_mode = 2
}ShutdownMode;

//函数指针 和 函数参数
struct ThreadTask {
    std::function< void(std::shared_ptr<void>) > process;// 实际传入的是Server::do_request;
    std::shared_ptr<void> arg;// 实际应该是HttpData对象
};

class ThreadPool {
public:
    ThreadPool(int thread_s,int max_queue_s);
    ~ThreadPool();
    bool append(std::shared_ptr<void>arg,std::function<void(std::shared_ptr<void>)> fun);
    void shutdown(bool graceful);
private:
    ///工作线程运行的函数，它不断从工作队列取出任务并执行
    static void *worker(void *args);
    void run();
private:
    //线程同步互斥
    MutexLock m_mutex;
    cond m_cond;

    //线程池属性
    int m_thread_size;
    int max_queue_size;
    int started;

    ///线程池是否可用
    int shutdown_;
    std::vector<pthread_t> threads;
    std::list<ThreadTask> request_queue;
};
#endif // THREADPOOL_H_INCLUDED
