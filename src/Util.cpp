/********************************************************
*@file   Util.cpp
*@Author herongwei
*@Data   2019/03/31
*********************************************************/
#include "Util.h"
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <sys/stat.h>

std::string& ltrim(std::string &str) {
    if(str.empty()) {
        return str;
    }
    //Searches the string for the first character that does not match any of the characters specified in its arguments.
    str.erase(0,str.find_first_not_of(" \t"));
    return str;
}
std::string& rtrim(std::string &str) {
    if(str.empty()) {
        return str;
    }
    str.erase(0,str.find_last_not_of(" \t")+1);
    return str;
}
//除去制表符
std::string& trim(std::string& str) {
    if (str.empty()) {
        return str;
    }
    ltrim(str);
    rtrim(str);
    return str;
}
//设置 fd 非阻塞
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
//信号处理函数
void handle_for_sigpipe()
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;//忽略目标信号
    sa.sa_flags = 0;
    //sig指出要捕获的信号类型；act参数指定新的信号处理方式；oact参数则输出先前的处理方式
    if(sigaction(SIGPIPE, &sa, NULL))
        return;
}

int check_base_path(char *basePath) {
    struct stat file;
    if (stat(basePath, &file) == -1) {
        return -1;
    }
    // 不是目录 或者不可访问
    if (!S_ISDIR(file.st_mode) || access(basePath, R_OK) == -1) {
        return -1;
    }
    return 0;
}
