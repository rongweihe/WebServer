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
  desc: simple C threadpool achieve
*/

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

const int DEFAULT_TIME        = 10; //�������̶߳�ʱ�����У��߳�״̬��ʱ����
const int MIN_WAIT_TASK_NUM   = 10; //�����еȴ���������
const int DEFAULT_THREAD_VARY = 10; //ÿ�δ������߳���Ŀ
using namespace std;

typedef struct
{
  void* (*function) (void *);         //����ָ��
  void *arg;                          //���溯������
}threadpool_task_t;                   //�����߳�����ṹ��

struct threadpool_t
{
    pthread_mutex_t lock;             //��������������ס���ṹ��
    pthread_mutex_t thread_count;     //��¼æ״̬�̸߳��� ---������busy_thread_num
    pthread_cond_t queue_not_full;   //����������������������߳������������ͻ��ˣ������������������ȴ���������������
    pthread_cond_t queue_not_empty;  //��������в�Ϊ�գ�֪ͨ�ȴ�������̣߳���������Ľ�������

    pthread_t *threads;               //����̳߳���ÿ���̵߳� tid ����
    pthread_t admin_tid;             //������߳�tid
    threadpool_task_t *task_queue;    //�������

    int min_thread_num;               //�̳߳���С�߳���
    int max_thread_num;               //�̳߳�����߳���
    int live_thread_num;              //�̳߳ش���߳���
    int busy_thread_num;              //�̳߳�æ״̬�߳���
    int wait_exit_thread_num;         //�̳߳صȴ������߳���

    int queue_front;                  //task_queue��ͷ�±�
    int queue_rear;                   //task_queue��β�±�
    int queue_size;                   //task_queueʵ��������
    int queue_max_size;               //task_queue����������������

    bool shutdown;                     //��־λ���̳߳�ʹ��״̬��true��false
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
 * free a threadpool_t �ͷ��̳߳�
 */
int threadpool_free(threadpool_t *pool);

////�����̳߳�
//threadpool_t *threadpool_create(int min_thr_num,int max_thr_num, int queue_max_size);
//
//// ��������ӵ����������
////int threadpool_add(threadpool_t *pool, void*(*function)(void *arg),void *arg);
//
////�̳߳��и��������̣߳���������в�Ϊ�գ�ȥ���������ȡ����
//void *threadpool_thread(void *threadpool);
//
////�������߳�
//void *admin_thread(void *threadpool);
//
////�����̳߳�
//int threadpool_destroy(threadpool_t *pool);
//
////
//int threadpool_free(threadpool_t *pool);

//�����̳߳�
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
        pool->live_thread_num = min_thr_num; // ���ŵ��߳��� ��ʼֵ= ��С�߳�ֵ
        pool->queue_size      = 0;
        pool->queue_max_size  = queue_max_size;
        pool->queue_front     = 0;
        pool->queue_rear      = 0;
        pool->shutdown        = false;      //�����̳߳�

        //��������߳����������������߳����鿪�ٿռ䣬������
        pool->threads = (pthread_t *) malloc (sizeof(pthread_t)*max_thr_num);
        if(pool->threads == NULL )
        {
            printf("malloc threads failed");
            break;
        }
        memset(pool->threads, 0, sizeof(pthread_t)*max_thr_num);

        //���п��ٿռ�,ÿ�������һ�� threadpool_task_t ���ͣ����� function �� arg
        //ͨ�� malloc �Ϳ���������һ���ڴ棬�������ÿһ��Ԫ�ض��� threadpool_task_t ���ͣ�Ҳ����˵
        //ÿһ��������һ���ṹ��
        pool->task_queue = (threadpool_task_t *) malloc(sizeof(threadpool_task_t)*queue_max_size);
        if(pool->task_queue == NULL)
        {
            printf("malloc task_queue failed");
            break;
        }

        //��ʼ������������������
        if (pthread_mutex_init(&(pool->lock), NULL) != 0
		|| pthread_mutex_init(&(pool->thread_count), NULL) != 0
		|| pthread_cond_init(&(pool->queue_not_empty), NULL) != 0
		|| pthread_cond_init(&(pool->queue_not_full), NULL) != 0)
		{
			printf("init the lock or cond fail");
			break;
		}

        //���� min_thr_num �� work thread
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
֮����д�ɡ�if (NULL==dst||NULL ==src)��������д�ɡ�if (dst == NULL || src == NULL)����
Ҳ��Ϊ�˽��ͷ�����ĸ��ʡ�����֪������C�������桰==���͡�=�����ǺϷ����������
������ǲ�С��д���ˡ�if (dst = NULL || src = NULL)�����ǿ��Ա���ͨ��������˼ȴ��ȫ��һ���ˣ�
�������д�ɡ�if (NULL=dst||NULL =src)����������ʱ���ͨ�����ˣ�
��������Ҫ�������õĳ������ϰ�ߣ�����������������ж�ʱӦ�ðѳ���д��ǰ�档
*/
// ��������ӵ����������
int threadpool_add(threadpool_t *pool, void*(*function)(void *arg), void *arg)
{
	assert(pool != NULL);
	assert(function != NULL);
	assert(arg != NULL);
	pthread_mutex_lock(&(pool->lock));
	//��������ʱ�򣬲���������񣬵ȴ�
	while ((pool->queue_size == pool->queue_max_size) && (!pool->shutdown))
	{
		//queue full wait
		pthread_cond_wait(&(pool->queue_not_full), &(pool->lock));
	}
	//����̳߳ز����ã�����
	if (pool->shutdown)
	{
		pthread_mutex_unlock(&(pool->lock));
	}
	//������������񵽶��У�ʹ��ѭ������
	//��չ����̵߳Ļص������Ĳ���arg
	if (pool->task_queue[pool->queue_rear].arg != NULL)
	{
		free(pool->task_queue[pool->queue_rear].arg);
		pool->task_queue[pool->queue_rear].arg = NULL;
	}
	pool->task_queue[pool->queue_rear].function = function;
	pool->task_queue[pool->queue_rear].arg = arg;
	//��Ӳ���
	pool->queue_rear = (pool->queue_rear + 1)%pool->queue_max_size;
	pool->queue_size++;
	//ÿ�μ������񣬷����źŸ��߳�,��û���̴߳��ڵȴ�״̬
	pthread_cond_signal(&(pool->queue_not_empty));
	pthread_mutex_unlock(&(pool->lock));
	return 0;
}

//�̳߳��и��������̣߳���������в�Ϊ�գ�ȥ���������ȡ����
void *threadpool_thread(void *threadpool)
{
    //��ʼ����Ϣ
    threadpool_t *pool = (threadpool_t *)threadpool;
    threadpool_task_t task;
    while(true)
    {
        //�մ������̣߳��ȴ���������������񣬷������������������������ٻ��ѽ�������
        //����
        pthread_mutex_lock(&(pool->lock));

        //˵��û��������� pthread_cond_wait ���������������ϣ������������� while
        while( (pool->queue_size ==0) && (!pool->shutdown) )
        {
            //gettid ��ȡ�����ں����߳�ID����pthread_self ��posix�������߳�ID��
            printf("thread 0x%x is waiting\n",(unsigned int)pthread_self());
            //�߳̿��У�����
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->lock));

            //���ָ����Ŀ�Ŀ����̣߳����Ҫ�������̴߳��� 0 ������߳�
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

        //���ָ����true��Ҫ�ر��̳߳ص�ÿ���̣߳������˳�����
        if(pool->shutdown)
        {
            pthread_mutex_unlock(&(pool->lock));
            printf("thread 0x%x is exiting\n",(unsigned int) pthread_self() );
            pthread_exit(NULL);
        }

        //������������ȡ������һ�����Ӳ���
        task.function = pool->task_queue[pool->queue_front].function;
        task.arg = pool->task_queue[pool->queue_front].arg;

        //���ӣ�ģ�⻷�ζ���
        pool->queue_front = (pool->queue_front + 1) % pool->queue_max_size;
        pool->queue_max_size--;

        //֪ͨ�������µ�������ӽ��������ѱ������Ŀͻ���
        pthread_cond_broadcast(&(pool->queue_not_full));
        //����ȡ���������ͷ��̳߳���
        pthread_mutex_unlock(&(pool->lock));

        //ִ������
        printf("thread 0x%x start working\n",(unsigned int) pthread_self() );
        //æ״̬�߳�������
        pthread_mutex_lock(&(pool->thread_count));
        pool->busy_thread_num++;
        pthread_mutex_unlock(&(pool->thread_count));
        //ִ�лص�����
        (*(task.function))(task.arg);

        //�����������
        printf("thread 0x%x end working\n",(unsigned int) pthread_self() );
        pthread_mutex_lock(&(pool->thread_count));
        pool->busy_thread_num--;
        pthread_mutex_unlock(&(pool->thread_count));
    }
    pthread_exit(NULL);
    return NULL;
}

//�������߳�
void *admin_thread(void *threadpool)
{
    threadpool_t *pool = (threadpool_t *)threadpool;
    //threadpool_task_t task;
    while(!pool->shutdown)
    {
        //��ʱ���̳߳ع���
        sleep(DEFAULT_TIME);

        //��һ�����ݼ�⣻��ȡ�������ʹ���߳���
        pthread_mutex_lock(&(pool->lock));
        int queue_size   = pool->queue_size;
        int live_thr_num = pool->live_thread_num;
        pthread_mutex_unlock(&(pool->lock));

        //��ȡæ״̬�߳�
        pthread_mutex_lock(&(pool->thread_count));
        int busy_thr_num = pool->busy_thread_num;
        pthread_mutex_unlock(&(pool->thread_count));

        //�����߳��㷨��������������С�̳߳ظ������Ҵ����߳�����������߳���
        if(queue_size >= MIN_WAIT_TASK_NUM && live_thr_num < pool->max_thread_num)
        {
            pthread_mutex_lock(&(pool->lock));
            int add = 0;
            //һ������ DEFAULT_THREAD ���߳�
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
        //���ٶ���Ŀ����̣߳�æ�߳�*2 < �����߳��� �� �����߳��� > ��С�߳���
        if( (busy_thr_num*2)<live_thr_num && live_thr_num > pool->min_thread_num )
        {
            //һ�������� DEFAULT_THREAD_VARY �߳�
            pthread_mutex_lock(&(pool->lock));
            pool->wait_exit_thread_num = DEFAULT_THREAD_VARY;
            pthread_mutex_unlock(&(pool->lock));
            //֪ͨ���ڿ���״̬���̣߳���������
            //[1]�ַ�������ɱ�˲���Ѫ�����ֱ��ɱ��������մ��������Ѫ����ֱ��ɱ�������ÿ����߳������˽�
            //[2]��������߳��кܶ࣬��� pthread_cond_wait ʹ�����Ǵ�������״̬
            //[3]��� pthread_cond_signal ���Ѵ�������״̬�Ŀ����߳�
            //[4]��������״̬�Ŀ����߳�һ�������ѣ������߳̾ͻ�ȥ���������������
            //[5]�����߳���ô�࣬��ֻ��һ��������ֻ��һ���߳��������������������̼߳������½���
            //[6]ִ�е�������� if(pool->wait_exit_thread_num >0 ) pool->wait_exit_thread_num-- ��ʱ��
            //[7]���������߳�һ�����ҿ���ԭ����Ҫ����ִ����������Ҫ��ֱ���˽Ⱑ���൱���ں�һ�����ڵȴ���������ȥ����û�취������pthread_exit �����˽��
            //[8]ʣ�µ�û�����������̻߳������������Թ������߳�ѭ����DEFAULT_THREAD_VARY�Σ��൱�ڻ�������ô��Σ��Ӷ��ﵽ�������п����̵߳�����Ŀ��
            for(int i=0; i<DEFAULT_THREAD_VARY; ++i)
            {
                pthread_cond_signal(&(pool->queue_not_empty));
            }
        }
    }
    return NULL;
}

//�����̳߳�
int threadpool_destroy(threadpool_t *pool)
{
    if(pool==NULL)
    {
        return -1;
    }
    pool->shutdown = true;

    //�����ٹ������߳�
    pthread_join(pool->admin_tid,NULL);

    //֪ͨ���п����߳�
    for(int i=0; i<pool->live_thread_num; ++i)
    {
        pthread_cond_broadcast(&(pool->queue_not_empty));
    }
    //�ȴ��̵߳Ľ���
    for(int i=0; i<pool->live_thread_num; ++i)
    {
        pthread_join(pool->threads[i],NULL);
    }
    threadpool_free(pool);
    return 0;
}

//�ͷ��̳߳�
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
    //��0�źţ������߳��Ƿ���
    int kill_rc = pthread_kill(tid,0);
    if(kill_rc == ESRCH){
        return false;
    }
    return true;
}

//�̳߳��е��̣߳�ģ�⴦��ҵ��
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
    threadpool_t *thp = threadpool_create(3,100,12);//�����̳߳أ���С3���̣߳����100���������100
    printf("pool create");

    int *num = (int *)malloc(sizeof(int)*20);
    for(int i=0; i<10; ++i)
    {
        num[i]=i;
        printf("add task %d\n",i);
        threadpool_add(thp,process,(void*)&num[i]); //���̳߳����������
    }
    sleep(10);
    threadpool_destroy(thp);                        //�����߳��������
}
