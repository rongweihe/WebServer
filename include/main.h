#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#include <bits/stdc++.h>

void daemon_run()
{
    int pid;
    signal(SIGCHLD,SIG_IGN);
    /*
    [1] 在父进程中，fork 返回创建子进程的进程 ID
    [2] 在子进程中，fork 返回 0
    [3] 如果 fork 出错，返回一个负值
    */
    pid = fork();
    if(pid<0) {
        std::cout<<"fork error"<<endl;
        exit(-1);
    } else if(pid>0) {
        exit(0);
    }
    //父进程退出，子进程独立运行

    /*
    [1]之前父子进程是运行在同一个session会话里，父进程是会话的领头进程。
    [2]父进程作为会话的领头进程，如果exit结束执行的话，那么子进程会成为孤儿进程，并被init进程收养
    [3]setsid之后，子进程将重新获得一个新的会话id，这时父进程退出以后，将不会影响到子进程
    */
    setsid();

    /*
    open 函数用来打开一个设备
    返回的是一个整型变量
    [1]如果这个值等于-1，说明打开文件出现错误
    [2]如果为大于0的值，那么这个值代表的就是文件描述符。
    O_RDONLY 只读打开。
    O_WRONLY 只写打开。
    O_RDWR 读、写打开。
    O_APPEND 每次写时都加到文件的尾端。
    */
    int fd;
    fd = open("dev/null",O_RDWR,0);

    /*
    将标准输入，输出，错误重定向到 /dev/null 文件
    */
    if(fd!=-1)
    {
        dup2(fd,STDIN_FILENO);
        dup2(fd,STDOUT_FILENO);
        dup2(fd,STDERR_FILENO);
    }


    /*关闭标准输入，输出，错误设备*/
    if(fd>2)
        close(fd);
}

#endif // MAIN_H_INCLUDED
