/********************************************************
*@file   main.cpp
*@Author herongwei
*@Data   2019/03/31
*********************************************************/
#include "Server.h"
#include "Util.h"

#include <string>
#include <iostream>
#include <dirent.h>
#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>  //for signal


//程序默认当前目录
std::string basePath = ".";

//进程以守护进程的方式运行
void daemon_run()
{
    //创建子进程，关闭父进程，这样可以使程序在后台运行
    //1）在父进程中，fork返回新创建子进程的进程ID；
    //2）在子进程中，fork返回0；
    //3）如果出现错误，fork返回一个负值；

    signal(SIGCHLD,SIG_IGN);
    pid_t pid = fork();
    if(pid<0)
    {
        std::cout<<"fork eror"<<std::endl;
        exit(-1);
    }
    //父进程退出，子进程独立运行
    else if(pid>0)
    {
        exit(0);
    }
    //之前parent和child运行在同一个session里,parent是会话（session）的领头进程,
    //parent进程作为会话的领头进程，如果exit结束执行的话，那么子进程会成为孤儿进程，并被init收养。
    //执行setsid()之后,child将重新获得一个新的会话(session)id。
    //这时parent退出之后,将不会影响到child了。
    setsid();
    //关闭标准输入，输出，错误
    int fd = open("dev/null",O_RDWR,0);
    if(fd!=-1)
    {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
    }
    if (fd > 2)
    {
        close(fd);
    }
    /*
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("dev/null",O_RDONLY,0);
    open("dev/null",O_RDWR,0);
    */
}
int main(int argc, const char *argv[])
{
    //默认线程数
    int thread_number = 4;
    int port = 6666;
    char tp_path[256];
    int opt;
    const char *str ="t:p:r:d";
    bool daemon = false;

    //getopt 解析命令行参数函数
    while((opt=getopt(argc,argv,str))!=-1)
    {
        switch(opt)
        {
        case 't':
        {
            thread_number = atoi(optarg);
            break;
        }
        case 'r':
        {
            //疑问
            int ret = check_base_path(optarg);
            if(ret==-1)
            {
                printf("Warning: \"%s\" 不存在或不可访问, 将使用当前目录作为网站根目录\n", optarg);
                //将当前工作目录的绝对路径复制到参数buffer所指的内存空间中
                if(getcwd(tp_path,256) == NULL)
                {
                    perror("getcwd error");
                    basePath = ".";
                }
                else
                {
                    basePath = tp_path;
                }
                break;
            }
            //疑问
            if (optarg[strlen(optarg)-1] == '/')
            {
                optarg[strlen(optarg)-1] = '\0';
            }
            basePath = optarg;
            break;
        }
        case 'p':
        {
            // FIXME 端口合法性校验
            port = atoi(optarg);
            break;
        }
        case 'd':
        {
            daemon = true;
            break;
        }
        default:
            break;
        }
    }
    //守护进程
    if(daemon)
    {
        daemon_run();
    }
    //  输出配置信息
    {
      printf("*******WebServer 配置信息*******\n");
      printf("端口:\t%d\n", port);
      printf("线程数:\t%d\n", threadNumber);
      printf("根目录:\t%s\n", basePath.c_str());
    }
    //疑问
    handle_for_sigpipe();

   // 绑定端口，启动
    HttpServer httpServer(port);
    httpServer.run(threadNumber);
    return 0;
    /*
    http_request
    HTTP 请求
    打开文件的时候需要判断一下，打开的是文件还是目录
    获取文件的属性
    stat() 函数
    struct stat st;
    //第二个属性传出参数
    int ret = stat(file, &st);
    if(ret == -1)
    {
     show(404);
    }
    */
}

