#ifndef MUTEXLOCK_H_INCLUDED
#define MUTEXLOCK_H_INCLUDED

#include <pthread.h>
#include "noncopyable.h"

class MutexLock : public noncopyable {
public:
    //创建初始化互斥锁
    MutexLock() {
        if(pthread_mutex_init(&m_mutex,NULL) !=0)
        {
            throw std::exception();
        }
    }
    //销毁互斥锁
    ~MutexLock(){
         pthread_mutex_destroy(&m_mutex);
    }
    //获取互斥锁
    void lock() {
        pthread_mutex_lock(&m_mutex) ;
    }
    //释放互斥锁
    void unlock() {
        pthread_mutex_unlock(&m_mutex) ;
    }
    //获取返回值
    pthread_mutex_t* getMutex() {
        return &m_mutex;
    }
private:
    pthread_mutex_t m_mutex;
};
class MutexLockGuard : public noncopyable {
public:
    explicit MutexLockGuard(MutexLock &mutex) : m_mutex(m_mutex) {
      m_mutex.lock();
    }
    ~MutexLockGuard(){
        m_mutex.unlock();
    }
private:
    MutexLock m_mutex;
};
#endif // MUTEXLOCK_H_INCLUDED
