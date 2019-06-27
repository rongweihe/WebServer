
/************************************************************************************************
         < 半同步/半异步并发模式的进程池实现
         < filename: processpool.h
         < 特征
         < 1. 为了避免在父子进程之间传递文件描述符，将接受新连接的操作放到子进程中
         < 2. 一个客户连接上的的所有任务始终是由一个字进程来处理的
         < Author:rongweihe
         < Mail:rongweihe1995@gmail.com
         < Date: 2019-03-20
************************************************************************************************/
#ifndef processpool_H
#define processpool_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>

/*描述一个子进程的类，m_pid 是目标子进程的 PID，m_pipefd 是父进程和子进程通信用的管道*/
class process
{
public:
    process() : m_pid( -1 ) {}
public:
    pid_t m_pid;
    int m_pipefd[2];
};
/*进程池类，将它定义为模板类是为了代码复用，其模板参数是处理逻辑任务的类*/
template < typename T>
class processpool
{
private:
    /*将构造函数定义为私有的，因此我们只能通过后面的 create 静态函数来创建 processpool 类*/
    processpool( int listenfd,int process_number = 8 );
public:
    /*单体模式，以保证程序最多创建一个 processpool 实例，这是程序正确处理信号的必要条件*/
    static processpool< T >* creat( int listenfd,int process_number = 8 )
    {
        if ( !m_instance )
        {
            m_instance = new processpool < T >(listenfd,process_number)
        }
        return m_instance;
    }
    ~processpool()
    {
        delete [] m_sub_process;
    }
    /*启动进程池*/
    void run();
private:
    void setup_sig_pipe();
    void run_parent();
    void run_child();
private:
    /*进程池允许的最大子进程数量*/
    static const int MAX_PROCESS_NUMBER   = 16;

    /*每个子进程最多能处理的客户数量*/
    static const int MAX_USER_PER_PROCESS = 65536;

    /*epoll 最多能处理的事件数*/
    static const int MAX_EVENT_NUMBER     = 10000;

    /*进程池中的进程数*/
    int m_process_number;

    /*子进程在池中的序号从 0 开始*/
    int m_idx;

    /*每个进程都有一个 epoll 内核事件表，用 m_epollfd 标识*/
    int m_epollfd;

    /*监听 socket */
    int m_listenfd;

    /*子进程通过 m_stop 来决定是否停止运行*/
    int m_stop;

    /*保存所有子进程的描述信息*/
    process* m_sub_process;

    /*进程池静态实例*/
    static processpool<T>* m_instance;
};
template <typename T>
processpool<T>* processpool<T>::m_instance = NULL;

/*用于处理信号的管道，以实现统一事件源，后面称之为信号管道*/
static int sig_pipefd[2];
static int setnonblocking(int fd)
{
    int old_option = fcntl(fd,F_GETEL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}
static void addfd(int epollfd,int fd)
{
    epoll_event event;
    event.data.fd = fd;
    /*采用epoll的EPOLLET（边沿触发）模式*/
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}
/*从 epollfd 标识的 epoll 内核事件表中删除 fd 上的所有注册事件*/
static void removefd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}
static void sig_handler(int sig)   ///?
{
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1],(char*)&msg,1,0);
    errno = save_errno;
}
static void addsig(int sig,void(handler)(int),bool restart = true)
{
    /*信号安装函数sigaction(int signum,const struct sigaction *act,struct sigaction *oldact)*/
    struct sigaction sa;
    memset(&sa, '\0',sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
    {
        sa.sa_flags | = SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NNULL) != -1);
}
/*进程池构造函数，参数 listenfd 是监听 socket，他必须在创建进程池之前被创建，否则子进程无法直接引用它，参数 process_number 指定进程池中子进程的数量*/
template<typename T>
processpool<T>::processpool(int listenfd,int process_number)
    :m_listenfd(listenfd),m_process_number(process_number),m_idx(-1),m_stop(false)
{
    assert( (process_number >0) && (process_number <= MAX_PROCESS_NUMBER) );
    m_sub_process = new process[process_number];
    assert(m_sub_process);
    /*创建 process_number 个子进程，并建立它们和父进程之间的管道*/
    for(int i=0; i< process_number; ++i)
    {
        int ret = socketpair(PF_UNIX,SOCK_STREAM,0,m_sub_process[i].m_pipefd);
        assert(ret==0);
        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid>=0);
        if(m_sub_process[i].m_pid>0)
        {
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        else
        {
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;///子进程赋予自己的index
            break;///这才是进程池的精华子进程需要退出循环，不然子进程也会fork
        }
    }
}

/*统一事件源，这个pipe不是用来和父进程通信的pipe*/
template <typename T>
void processpool<T>::setup_sig_pipe()
{
    /*创建epoll事件监听表和信号管道*/
    m_epollfd = epoll_creat(5);
    assert( m_epollfd !=-1 );
    int ret = socketpair(PF_UNIX,SOCK_STREAM,0,sig_pipefd);
    assert(ret!=-1);
    setnonblocking(sig_pipefd[1]);
    addfd(m_epollfd,sig_pipefd[0]);
    /*设置信号处理函数*/
    addsig(SIGCHLD,sig_handler);
    addsig(SIGTERM,sig_handler);
    addsig(SIGINT,sig_handler);
    addsig(SIGPIPE,SIG_IGN);
}
/*父进程中 m_idx = -1,子进程 m_idx 大于等于0 我们要据此判断接下来要运行的是父进程代码还是子进程代码*/
template<typename T>
void processpool<T>::run()
{
    if(m_idx!=-1)
    {
        run_child();
        return;
    }
    run_parent();
}
void processpool<T>::run_child()
{
    setup_sig_pipe();
    /*每个子进程都通过其在进程池中的序号值 m_idx 找到与父进程通信的管道*/
    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    /*子进程需要监听管道文件描述符 pipefd，因为父进程将通过它来通知子进程 accept 新连接*/
    addfd(m_epollfd,pipefd);
    epoll_event events[MAX_EVENT_NUMBER];
    T* users = new T[MAX_USER_PER_PROCESS];
    assert(users);
    int number = 0;
    int ret = -1;
    while(!m_stop)
    {
        /*监听事件*/
        number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if( number <0 && errno!=EINTR )
        {
            printf("epoll error!\n");
            break;
        }
        for(int i=0; i<number; ++i)
        {
            int sockfd = events[i].data.fd;
            /*父进程有写管道表明有新连接需要该子进程处理*/
            if( sockfd == pipefd && (events[i].events & EPOLLIN))
            {
                int client = 0;
                /*从父子进程之间的管道中读取数据，并将结果保存在变量 client 中，如果读取成功，则表示有新客户连接到来*/
                ret = recv(sockfd, (char*)&client, sizeof(client),0);
                if( ((ret<0) && (errno != EAGAIN) ) || ret == 0 )
                {
                    continue;
                }
                else
                {
                    struct sockaddr_in client_address;
                    socklen_t client_addrlen = sizeof(client_address);
                    int connfd = accept(m_listenfd, (struct sockaddr*)&client_address,&client_addrlen);
                    if(connfd <0 )
                    {
                        printf("accept errno is: %d\n",errno);
                        continue;
                    }
                    addfd(m_epollfd,connfd);
                    /*模板类T必须实现 init 方法，以初始化一个客户连接，我们直接使用 connfd 来索引逻辑处理对象（T类型的对象）以提高程序效率*/
                    users[connfd].init(m_epollfd,connfd,client_address);
                }
            }
            else if( (sockfd == sig_pipefd[0] ) && ( events[i].events & EPOLLIN ))
            {
                /*处理子进程接受到的任务*/
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0],signals,sizeof(signals),0);
                if(ret<=0)
                {
                    continue;
                }
                else
                {
                    for(int i=0; i<ret; ++i)
                    {
                        switch(signals[i])
                        {
                        case SIGCHLD:
                        {
                            pid_t pid;
                            int stat;
                            /*
                            * 为了防止僵尸进程和孤儿进程两种情况，我们应当在父进程结束之前一定要回收子进程的所有资源
                            * 所以出现了wait和waitpid
                            *status是一个传出参数。
                            waitpid的pid参数选择：
                            < -1 回收指定进程组内的任意子进程
                            = -1 回收任意子进程
                            = 0  回收和当前调用waitpid一个组的所有子进程
                            > 0  回收指定ID的子进程
                            *
                            */
                            while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )
                            {
                                continue;
                            }
                            break;
                        }
                        case SIGTERM:
                        case SIGINT:
                        {
                            m_stop = true;
                            break;
                        }
                        default:
                        {
                            break;
                        }
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN)
            {
                users[sockfd].process();
            }
            else
            {
                continue;
            }
        }
    }
    delete [] users;
    users = NULL;
    close(pipefd);
    close(m_epollfd);
    //close( m_listenfd );
    /*我们将上面这句话注释掉，以提醒读者：应该由m_listenfd的创建者来关闭这个文件描述符，及所谓的“对象由哪个函数创建，就应该由哪个函数销毁”*/
}
template<typename T>
void processpool<T>::run_parent()
{
    setup_sig_pipe();
    /*父进程监听 m_listenfd */
    addfd(m_epollfd,m_listenfd);
    epoll_event events[MAX_EVENT_NUMBER];
    int sub_porcess_counter = 0;
    int new_conn = 1;
    int number = 0;
    int ret = -1;
    while(!m_stop)
    {
        number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if( (number <0 ) && (errno != EINTR ))
        {
            printf("epoll error \n");
            break;
        }
        for(int i=0; i<number; ++i)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == m_listenfd)
            {
                /*如果有新进程到来，就采用 Round Robin 方式将其分配给一个子进程处理*/
                int i = sub_porcess_counter;
                do
                {
                    if(m_sub_process[i].m_pid !=-1 )
                    {
                        break;
                    }
                    i=(i+1)%m_process_number;
                }
                while(i!=sub_porcess_counter);
                if(m_sub_process[i].m_pid == -1)
                {
                    m_stop = true;
                    break;
                }
                sub_porcess_counter = (i+1)%m_process_number;
                send(m_sub_process[i].m_pipefd[0], (char*)&new_conn, sizeof(new_conn),0);
                printf("%send request to child %d\n",i);
            }
            else if(( sockfd == sig_pipefd[0] ) && ( events[i].events & EPOLLIN ))
            {
                /*下面处理父进程接受到的信号*/
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0],signals,sizeof(signals),0);
                if(ret<=0)
                {
                    continue;
                }
                else
                {
                    for(int i=0; i<ret; ++i)
                    {
                        switch(signals[i])
                        {
                        case SIGCHLD:
                        {
                            pid_t pid;
                            int stat;
                            /*
                            * 为了防止僵尸进程和孤儿进程两种情况，我们应当在父进程结束之前一定要回收子进程的所有资源
                            * 所以出现了wait和waitpid
                            *status是一个传出参数。
                            waitpid的pid参数选择：
                            < -1 回收指定进程组内的任意子进程
                            = -1 回收任意子进程
                            = 0  回收和当前调用waitpid一个组的所有子进程
                            > 0  回收指定ID的子进程
                            *
                            */
                            while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )
                            {
                                /*
                                如果进程池中某个第i个子进程退出了，则主进程关闭相应的通信管道，并设置相应的
                                m_pid = -1;以标记子进程已经退出
                                */
                                for(int i=0; i<m_process_number; ++i)
                                {
                                    if(m_sub_process[i].m_pid == pid)
                                    {
                                        printf("child %d join \n",i);
                                        close(m_sub_process[i].m_pipefd[0]);
                                        m_sub_process[i].m_pid = -1;
                                    }
                                }
                            }
                            /*如果所有子进程都已经退出了，则父进程也退出*/
                            m_stop = true;
                            for(int i=0; i<m_process_number; ++i)
                            {
                                if(m_sub_process[i].m_pid != -1)
                                    m_stop = false;
                            }
                            break;
                        }
                        case SIGTERM:
                        case SIGINT:
                        {
                            /*
                            如果父进程收到终止信号，那么就杀死所有子进程，并等待它们全部结束
                            当然，通知子进程结束更好的办法是向父子进程之间的通信管道发送特殊数据
                            */
                            printf("kill all the child now \n");
                            for(int i=0; i<m_process_number; ++i)
                            {
                                int pid = m_sub_process[i].m_pid;
                                if(pid!=-1)
                                {
                                    kill(pid,SIGTERM);
                                }
                            }
                            break;
                        }
                        default:
                        {
                            break;
                        }
                        }
                    }
                }
            } else {
                continue;
            }
        }
    }
    close(m_epollfd);
}
#endif
