/********************************************************
*@file  封装HttpParse类
*@Author rongweihe
*@Data 2019/04/18
*********************************************************/
#ifndef HTTPPARSE_H_INCLUDED
#define HTTPPARSE_H_INCLUDED

#include <unordered_map>
#include <string>
#include <iostream>

const char CR =  '\r' ;
const char LF =  '\n';
const char LINE_END = '\0';
#define PASS

class HttpRequest;
std::ostream &operator<<(std::ostream &, const HttpRequest &);

class HttpRequestParser
{
public:
    //行的读取状态：读取到一个完整的行、行出错和行数据暂且不完整
    enum LINE_STATE
    {
        LINE_OK=0,
        LINE_BAD,
        LINE_MORE
    };
    //解析状态
    enum PARSE_STATE
    {
        PARSE_REQUESTLINE = 0,
        PARSE_HEADER,
        PARSE_BODY
    };
    //服务器处理 HTTP 请求的可能结果
    enum HTTP_CODE
    {
        NO_REQUEST,/*NO_REQUEST表示请求不完整，需要读取客户数据*/
        GET_REQUEST,/*GET_REQUEST表示获得了一个完整的客户请求*/
        BAD_REQUEST,/*BAD_REQUEST表示客户请求有语法错误*/
        FORBIDDEN_REQUEST,/*表示客户对资源没有足够的访问权限*/
        INTERNAL_ERROR,/*表示服务器内部错误*/
        CLOSED_CONNECTION/*表示客户端已经关闭连接*/
    };

    //从状态机，用于解析一行内容
    static LINE_STATE parse_line(char *buffer, int &checked_index, int &read_index);

    //分析请求行
    static HTTP_CODE parse_requestline(char *line, PARSE_STATE &parse_state, HttpRequest &request);

    //分析头部
    static HTTP_CODE parse_headers(char *line, PARSE_STATE &parse_state, HttpRequest &request);

    static HTTP_CODE parse_body(char *body, HttpRequest &request);
    //分析HTTP请求的入口函数
    static HTTP_CODE
    parse_content(char *buffer, int &check_index, int &read_index, PARSE_STATE &parse_state, int &start_line,
                  HttpRequest &request);
};
#endif // HTTPPARSE_H_INCLUDED
