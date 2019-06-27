#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

const int DEFAULT_TIME        = 10; //管理者线程定时检查队列；线程状态的时间间隔
const int MIN_WAIT_TASK_NUM   = 10; //队列中等待的任务数
const int DEFAULT_THREAD_VARY = 10; //每次创建的线程数目
using namespace std;

typedef struct
{
  void* (*function) (void *);         //函数指针
  void *arg;                          //上面函数参数
}threadpool_task_t;                   //各子线程任务结构体

struct threadpool_t
{
    pthread_mutex_t lock;             //互斥锁：用于锁住本结构体
    pthread_mutex_t thread_count;     //记录忙状态线程个数 ---作用于busy_thread_num
    pthread_cond_t queue_not_full;   //当任务队列满，添加任务的线程阻塞，阻塞客户端；此条件变量上锁，等待此条件变量解锁
    pthread_cond_t queue_not_empty;  //当任务队列不为空，通知等待任务的线程；消费任务的进程阻塞

    pthread_t *threads;               //存放线程池中每个线程的 tid 数组
    pthread_t admin_tid;             //存管理线程tid
    threadpool_task_t *task_queue;    //任务队列

    int min_thread_num;               //线程池最小线程数
    int max_thread_num;               //线程池最大线程数
    int live_thread_num;              //线程池存活线程数
    int busy_thread_num;              //线程池忙状态线程数
    int wait_exit_thread_num;         //线程池等待销毁线程数

    int queue_front;                  //task_queue队头下标
    int queue_rear;                   //task_queue队尾下标
    int queue_size;                   //task_queue实际任务数
    int queue_max_size;               //task_queue可容纳任务数上限

    bool shutdown;                     //标志位，线程池使用状态，true或false
};

/**
 * @function void *threadpool_thread(void *threadpool)
 * @desc the worker thread
 * @param threadpool the pool which own the thread
 */
void *threadpool_thread(void *threadpool);

/**
 * @function void *admin_thread(void *threadpool);
 * @desc manager thread
 * @param threadpool the threadpool
 */
void *admin_thread(void *threadpool);

/**
 * check a thread is alive
 */
bool is_thread_alive(pthread_t tid);

/**
 * free a threadpool_t 释放线程池
 */
int threadpool_free(threadpool_t *pool);

////创建线程池
//threadpool_t *threadpool_create(int min_thr_num,int max_thr_num, int queue_max_size);
//
//// 将任务添加到任务队列中
////int threadpool_add(threadpool_t *pool, void*(*function)(void *arg),void *arg);
//
////线程池中各个工作线程：当任务队列不为空：去任务队列领取任务
//void *threadpool_thread(void *threadpool);
//
////管理者线程
//void *admin_thread(void *threadpool);
//
////销毁线程池
//int threadpool_destroy(threadpool_t *pool);
//
////
//int threadpool_free(threadpool_t *pool);

//创建线程池
threadpool_t *threadpool_create(int min_thr_num,int max_thr_num, int queue_max_size)
{
    int i;
    threadpool_t *pool = NULL;
    do{
        if( (pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL )
        {
            printf("malloc threadpool failed");
            break;
        }
        pool->min_thread_num  = min_thr_num;
        pool->max_thread_num  = max_thr_num;
        pool->busy_thread_num = 0;
        pool->live_thread_num = min_thr_num; // 活着的线程数 初始值= 最小线程值
        pool->queue_size      = 0;
        pool->queue_max_size  = queue_max_size;
        pool->queue_front     = 0;
        pool->queue_rear      = 0;
        pool->shutdown        = false;      //开启线程池

        //根据最大线程上限数，给工作线程数组开辟空间，并清零
        pool->threads = (pthread_t *) malloc (sizeof(pthread_t)*max_thr_num);
        if(pool->threads == NULL )
        {
            printf("malloc threads failed");
            break;
        }
        memset(pool->threads, 0, sizeof(pthread_t)*max_thr_num);

        //队列开辟空间,每个结点是一个 threadpool_task_t 类型，包含 function 和 arg
        //通过 malloc 就开辟了这样一块内存，这里面的每一个元素都是 threadpool_task_t 类型，也就是说
        //每一个任务都是一个结构体
        pool->task_queue = (threadpool_task_t *) malloc(sizeof(threadpool_task_t)*queue_max_size);
        if(pool->task_queue == NULL)
        {
            printf("malloc task_queue failed");
            break;
        }

        //初始化互斥锁，条件变量
        if (pthread_mutex_init(&(pool->lock), NULL) != 0
		|| pthread_mutex_init(&(pool->thread_count), NULL) != 0
		|| pthread_cond_init(&(pool->queue_not_empty), NULL) != 0
		|| pthread_cond_init(&(pool->queue_not_full), NULL) != 0)
		{
			printf("init the lock or cond fail");
			break;
		}

        //启动 min_thr_num 个 work thread
        for(int i=0; i<min_thr_num; ++i)
        {
            pthread_create( &(pool->threads[i]), NULL, threadpool_thread,(void*)pool);
            printf("start thread 0x%x...\n", (unsigned int) pool->threads[i]);
        }

        pthread_create(&(pool->admin_tid),NULL,admin_thread,(void*)pool);
        return pool;
    }while(0);
    threadpool_free(pool);
    return NULL;
}
/*
之所以写成“if (NULL==dst||NULL ==src)”而不是写成“if (dst == NULL || src == NULL)”，
也是为了降低犯错误的概率。我们知道，在C语言里面“==”和“=”都是合法的运算符，
如果我们不小心写成了“if (dst = NULL || src = NULL)”还是可以编译通过，而意思却完全不一样了，
但是如果写成“if (NULL=dst||NULL =src)”，则编译的时候就通不过了，
所以我们要养成良好的程序设计习惯：常量与变量作条件判断时应该把常量写在前面。
*/
// 将任务添加到任务队列中
int threadpool_add(threadpool_t *pool, void*(*function)(void *arg), void *arg)
{
	assert(pool != NULL);
	assert(function != NULL);
	assert(arg != NULL);
	pthread_mutex_lock(&(pool->lock));
	//队列满的时候，不能添加任务，等待
	while ((pool->queue_size == pool->queue_max_size) && (!pool->shutdown))
	{
		//queue full wait
		pthread_cond_wait(&(pool->queue_not_full), &(pool->lock));
	}
	//如果线程池不可用，解锁
	if (pool->shutdown)
	{
		pthread_mutex_unlock(&(pool->lock));
	}
	//如下是添加任务到队列，使用循环队列
	//清空工作线程的回调函数的参数arg
	if (pool->task_queue[pool->queue_rear].arg != NULL)
	{
		free(pool->task_queue[pool->queue_rear].arg);
		pool->task_queue[pool->queue_rear].arg = NULL;
	}
	pool->task_queue[pool->queue_rear].function = function;
	pool->task_queue[pool->queue_rear].arg = arg;
	//入队操作
	pool->queue_rear = (pool->queue_rear + 1)%pool->queue_max_size;
	pool->queue_size++;
	//每次加完任务，发个信号给线程,若没有线程处于等待状态
	pthread_cond_signal(&(pool->queue_not_empty));
	pthread_mutex_unlock(&(pool->lock));
	return 0;
}

//线程池中各个工作线程：当任务队列不为空：去任务队列领取任务
void *threadpool_thread(void *threadpool)
{
    //初始化信息
    threadpool_t *pool = (threadpool_t *)threadpool;
    threadpool_task_t task;
    while(true)
    {
        //刚创建出线程，等待任务队列里有任务，否则阻塞任务队列里有任务后再唤醒接受任务
        //加锁
        pthread_mutex_lock(&(pool->lock));

        //说明没有任务调用 pthread_cond_wait 阻塞在条件变量上，若有任务，跳过 while
        while( (pool->queue_size ==0) && (!pool->shutdown) )
        {
            //gettid 获取的是内核中线程ID，而pthread_self 是posix描述的线程ID。
            printf("thread 0x%x is waiting\n",(unsigned int)pthread_self());
            //线程空闲，阻塞
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->lock));

            //清楚指定数目的空闲线程，如果要结束的线程大于 0 则结束线程
            if(pool->wait_exit_thread_num >0 )
            {
                 pool->wait_exit_thread_num--;
                 if(pool->live_thread_num > pool->min_thread_num)
                 {
                     printf("thread 0x%x is exiting \n", (unsigned int) pthread_self());
                     pool->live_thread_num--;
                     pthread_mutex_unlock(&(pool->lock));
                     pthread_exit(NULL);
                 }
            }
        }

        //如果指定了true，要关闭线程池的每个线程，自行退出处理
        if(pool->shutdown)
        {
            pthread_mutex_unlock(&(pool->lock));
            printf("thread 0x%x is exiting\n",(unsigned int) pthread_self() );
            pthread_exit(NULL);
        }

        //从任务队列里获取任务；是一个出队操作
        task.function = pool->task_queue[pool->queue_front].function;
        task.arg = pool->task_queue[pool->queue_front].arg;

        //出队，模拟环形队列
        pool->queue_front = (pool->queue_front + 1) % pool->queue_max_size;
        pool->queue_max_size--;

        //通知可以有新的任务添加进来，唤醒被阻塞的客户端
        pthread_cond_broadcast(&(pool->queue_not_full));
        //任务取出后，立即释放线程池锁
        pthread_mutex_unlock(&(pool->lock));

        //执行任务
        printf("thread 0x%x start working\n",(unsigned int) pthread_self() );
        //忙状态线程数量锁
        pthread_mutex_lock(&(pool->thread_count));
        pool->busy_thread_num++;
        pthread_mutex_unlock(&(pool->thread_count));
        //执行回调函数
        (*(task.function))(task.arg);

        //任务结束处理
        printf("thread 0x%x end working\n",(unsigned int) pthread_self() );
        pthread_mutex_lock(&(pool->thread_count));
        pool->busy_thread_num--;
        pthread_mutex_unlock(&(pool->thread_count));
    }
    pthread_exit(NULL);
    return NULL;
}

//管理者线程
void *admin_thread(void *threadpool)
{
    threadpool_t *pool = (threadpool_t *)threadpool;
    //threadpool_task_t task;
    while(!pool->shutdown)
    {
        //定时对线程池管理
        sleep(DEFAULT_TIME);

        //做一个数据检测；获取任务数和存活线程数
        pthread_mutex_lock(&(pool->lock));
        int queue_size   = pool->queue_size;
        int live_thr_num = pool->live_thread_num;
        pthread_mutex_unlock(&(pool->lock));

        //获取忙状态线程
        pthread_mutex_lock(&(pool->thread_count));
        int busy_thr_num = pool->busy_thread_num;
        pthread_mutex_unlock(&(pool->thread_count));

        //创建线程算法：任务数大于最小线程池个数；且存活的线程数少于最大线程数
        if(queue_size >= MIN_WAIT_TASK_NUM && live_thr_num < pool->max_thread_num)
        {
            pthread_mutex_lock(&(pool->lock));
            int add = 0;
            //一次增加 DEFAULT_THREAD 个线程
            for(int i=0; i<pool->max_thread_num && add < DEFAULT_THREAD_VARY && pool->live_thread_num < pool->max_thread_num; ++i)
            {
                if(pool->threads[i]==0 || !is_thread_alive(pool->threads[i]))
                {
                    pthread_create(&(pool->threads[i]), NULL,threadpool_thread,(void*)pool);
                    ++add;
                    ++pool->live_thread_num;
                }
            }
             pthread_mutex_unlock(&(pool->lock));
        }
        //销毁多余的空闲线程：忙线程*2 < 存活的线程数 且 存活的线程数 > 最小线程数
        if( (busy_thr_num*2)<live_thr_num && live_thr_num > pool->min_thread_num )
        {
            //一次性销毁 DEFAULT_THREAD_VARY 线程
            pthread_mutex_lock(&(pool->lock));
            pool->wait_exit_thread_num = DEFAULT_THREAD_VARY;
            pthread_mutex_unlock(&(pool->lock));
            //通知处于空闲状态的线程，自行销毁
            //[1]手法高明，杀人不见血，如果直接杀，则满手沾满淋漓献血。不直接杀，而是让空闲线程自行了解
            //[2]假设空闲线程有很多，语句 pthread_cond_wait 使得它们处于阻塞状态
            //[3]语句 pthread_cond_signal 唤醒处于阻塞状态的空闲线程
            //[4]处于阻塞状态的空闲线程一旦被唤醒，阻塞线程就会去抢这个条件变量锁
            //[5]空闲线程这么多，锁只有一个，所以只有一个线程能抢到锁，抢到锁的线程继续往下进行
            //[6]执行到这条语句 if(pool->wait_exit_thread_num >0 ) pool->wait_exit_thread_num-- 的时候，
            //[7]抢到锁的线程一看，我靠，原本我要继续执行任务，现在要我直接了解啊，相当于挖好一个坑在等待着我跳进去啊，没办法。。。pthread_exit 自行了解吧
            //[8]剩下的没有抢到锁的线程还在阻塞，所以管理者线程循环了DEFAULT_THREAD_VARY次，相当于唤醒了这么多次，从而达到销毁所有空闲线程的最终目的
            for(int i=0; i<DEFAULT_THREAD_VARY; ++i)
            {
                pthread_cond_signal(&(pool->queue_not_empty));
            }
        }
    }
    return NULL;
}

//销毁线程池
int threadpool_destroy(threadpool_t *pool)
{
    if(pool==NULL)
    {
        return -1;
    }
    pool->shutdown = true;

    //先销毁管理者线程
    pthread_join(pool->admin_tid,NULL);

    //通知所有空闲线程
    for(int i=0; i<pool->live_thread_num; ++i)
    {
        pthread_cond_broadcast(&(pool->queue_not_empty));
    }
    //等待线程的结束
    for(int i=0; i<pool->live_thread_num; ++i)
    {
        pthread_join(pool->threads[i],NULL);
    }
    threadpool_free(pool);
    return 0;
}

//释放线程池
int threadpool_free(threadpool_t *pool)
{
    if(pool == NULL)
    {
        return -1;
    }
    if(pool->task_queue)
    {
        free(pool->task_queue);
    }
    if(pool->threads)
    {
        free(pool->threads);
        pthread_mutex_lock(&(pool->lock));
        pthread_mutex_destroy(&(pool->lock));

        pthread_mutex_lock(&(pool->thread_count));
        pthread_mutex_destroy(&(pool->thread_count));

        pthread_cond_destroy(&(pool->queue_not_empty));
        pthread_cond_destroy(&(pool->queue_not_full));
    }
    free(pool);
    pool = NULL;
    return 0;
}
int threadpool_all_live_threadnum(threadpool_t *pool)
{
    int all_live_threadnum = -1;
    pthread_mutex_lock(&(pool->lock));
    all_live_threadnum = pool->live_thread_num;
    pthread_mutex_unlock(&(pool->lock));
    return all_live_threadnum;
}

int threadpool_busy_threadnum(threadpool_t *pool)
{
    int all_busy_threadnum = -1;
    pthread_mutex_lock(&(pool->thread_count));
    all_busy_threadnum = pool->busy_thread_num;
    pthread_mutex_unlock(&(pool->thread_count));
    return all_busy_threadnum;
}

bool is_thread_alive(pthread_t tid)
{
    //发0信号，测试线程是否存活
    int kill_rc = pthread_kill(tid,0);
    if(kill_rc == ESRCH){
        return false;
    }
    return true;
}

//线程池中的线程；模拟处理业务
void *process(void *arg)
{
    printf("thread 0x%x working on task %d\n", (unsigned int)pthread_self(), *(int *)arg);
    sleep(1);
    printf("task %d is end\n",*(int*)arg);
    return NULL;
}

int main(int argc, const char *argv[])
{
    // threadpool_t *threadpool_create(int min_thread_num, int max_thread_num, int queue_max_size);
    threadpool_t *thp = threadpool_create(3,100,12);//创建线程池，最小3个线程；最大100，队列最大100
    printf("pool create");

    int *num = (int *)malloc(sizeof(int)*20);
    for(int i=0; i<10; ++i)
    {
        num[i]=i;
        printf("add task %d\n",i);
        threadpool_add(thp,process,(void*)&num[i]); //向线程池中添加任务
    }
    sleep(10);
    threadpool_destroy(thp);                        //等子线程完成任务
}
