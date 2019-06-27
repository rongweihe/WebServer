/********************************************************
*@file  封装参数类
*@Author rongweihe
*
*********************************************************/
#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED
#include <string>
using namespace std;

std::string &ltrim(string &);
std::string &rtrim(string &);
std::string &trim(string &);

int setnonblocking(int fd);///
void handle_for_sigpipe();///
int check_base_path(char *basePath); ///

#endif // UTIL_H_INCLUDED
