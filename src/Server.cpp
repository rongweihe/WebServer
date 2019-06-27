/********************************************************
*@file   Server.cpp
*@Author rongweihe
*@Data   2019/03/31
*********************************************************/
#include "Server.h"
#include "HttpParse.h"
#include "HttpData.h"
#include "HttpRequest.h"
#include "ThreadPool.h"
#include "Epoll.h"
#include "Util.h"


#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
#include <string>
#include <functional>
#include <sys/epoll.h>
#include <vector>
#include <cstring>

char NOT_FOUND_PAGE[] = "<html>\n"
                        "<head><title>404 Not Found</title></head>\n"
                        "<body bgcolor=\"white\">\n"
                        "<center><h1>404 Not Found</h1></center>\n"
                        "<hr><center>Mini WebServer/0.3 (Linux)</center>\n"
                        "</body>\n"
                        "</html>";

char FORBIDDEN_PAGE[] = "<html>\n"
                        "<head><title>403 Forbidden</title></head>\n"
                        "<body bgcolor=\"white\">\n"
                        "<center><h1>403 Forbidden</h1></center>\n"
                        "<hr><center>Mini WebServer/0.3 (Linux)</center>\n"
                        "</body>\n"
                        "</html>";

char INDEX_PAGE[] = "<!DOCTYPE html>\n"
                    "<html>\n"
                    "<head>\n"
                    "    <title>Welcome to Mini WebServer!</title>\n"
                    "    <style>\n"
                    "        body {\n"
                    "            width: 35em;\n"
                    "            margin: 0 auto;\n"
                    "            font-family: Console, Verdana, Arial, sans-serif;\n"
                    "        }\n"
                    "    </style>\n"
                    "</head>\n"
                    "<body>\n"
                    "<h1>Welcome to Mini WebServer!</h1>\n"
                    "<p>If you see this page, the Mini webserver is successfully installed and\n"
                    "    working. </p>\n"
                    "\n"
                    "<p>For online documentation and support please refer to\n"
                    "    <a href=\"https://github.com/rongweihe/WebServer\">Mini WebServer</a>.<br/>\n"
                    "\n"
                    "<p><em>Thank you for using Mini WebServer.</em></p>\n"
                    "</body>\n"
                    "</html>";

char test[] = "HELLO WORLD";

extern std::string basePath;

void HttpServer::run(int thread_num, int MAX_QUEUE_SIZE)
{
    ThreadPool m_threadpool(thread_num,MAX_QUEUE_SIZE);

    int epoll_fd = Epoll::init(1024);

    std::shared_ptr<HttpData> m_http_data(new HttpData());
    m_http_data->epoll_fd = epoll_fd;
    serverSocket.epoll_fd = epoll_fd;

    __uint32_t event = (EPOLLIN | EPOLLET);
    Epoll::addfd(epoll_fd, serverSocket.listen_fd, event, m_http_data);

    while (true)
    {
        std::vector<std::shared_ptr<HttpData>> events = Epoll::poll(serverSocket, 1024, -1);
        // 将事件传递给 线程池
        for (auto& req : events)
        {
            threadPool.append(req, std::bind(&HttpServer::do_request, this, std::placeholders::_1));
        }
        // 处理定时器超时事件
        Epoll::timerManager.handle_expired_event();
    }
}

void HttpServer::do_request(std::shared_ptr<void> arg)
{
    std::shared_ptr<HttpData> m_shared_HttpData = std::static_pointer_cast<HttpData>(arg);

    char buffer[BUFFERSIZE];

    bzero(buffer, BUFFERSIZE);
    int check_index = 0, read_index = 0, start_line = 0;
    size_t recv_data;
    HttpRequestParser::PARSE_STATE  parse_state = HttpRequestParser::PARSE_REQUESTLINE;

    while (true)
    {
        recv_data = recv(m_shared_HttpData->clientSocket_->fd, buffer + read_index, BUFFERSIZE - read_index, 0);
        if (recv_data == -1)
        {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                return;
            }
            std::cout << "reading faild" << std::endl;
            return;
        }


        if (recv_data == 0)
        {
            std::cout << "connection closed by peer" << std::endl;
            break;
        }
        read_index += recv_data;

        HttpRequestParser::HTTP_CODE  retcode = HttpRequestParser::parse_content(
                buffer, check_index, read_index, parse_state, start_line, *m_shared_HttpData->request_);

        if (retcode == HttpRequestParser::NO_REQUEST)
        {
            continue;
        }


        if (retcode == HttpRequestParser::GET_REQUEST)
        {
            // 检查 keep_alive选项
            auto it = m_shared_HttpData->request_->mHeaders.find(HttpRequest::Connection);
            if (it != m_shared_HttpData->request_->mHeaders.end())
            {
                if (it->second == "keep-alive")
                {
                    m_shared_HttpData->response_->setKeepAlive(true);
                    m_shared_HttpData->response_->addHeader("Keep-Alive", std::string("timeout=20"));
                }
                else
                {
                    m_shared_HttpData->response_->setKeepAlive(false);
                }
            }
            header(m_shared_HttpData);
            get_mime(m_shared_HttpData);
            //
            FileState  fileState = static_file(m_shared_HttpData, basePath.c_str());
            send(m_shared_HttpData, fileState);
            // 如果是keep_alive else m_shared_HttpData将会自动析构释放clientSocket，从而关闭资源
            if (m_shared_HttpData->response_->keep_alive())
            {
                //std::cout << "再次添加定时器  keep_alive: " << m_shared_HttpData->clientSocket_->fd << std::endl;
                Epoll::modfd(m_shared_HttpData->epoll_fd, m_shared_HttpData->clientSocket_->fd, Epoll::kDefaultEvents, m_shared_HttpData);
                Epoll::timerManager.addTimer(m_shared_HttpData, TimerManager::DEFAULT_TIME_OUT);
            }
        }
        else
        {
            //  应该关闭定时器,(其实定时器已经关闭,在每接到一个新的数据时)
            std::cout << "Bad Request" << std::endl;
        }
    }
}

void HttpServer::header(std::shared_ptr<HttpData> httpData)
{
    if (httpData->request_->mVersion == HttpRequest::HTTP_11)
    {
        httpData->response_->setVersion(HttpRequest::HTTP_11);
    }
    else
    {
        httpData->response_->setVersion(HttpRequest::HTTP_10);
    }
    httpData->response_->addHeader("Server", "Mini WebServer");
}


// 获取  同时设置path到response
void HttpServer::get_mime(std::shared_ptr<HttpData> httpData)
{
    std::string filepath = httpData->request_->mUri;
    std::string mime;
    int pos;
//    std::cout << "uri: " << filepath << std::endl;
    if ((pos = filepath.rfind('?')) != std::string::npos)
    {
        filepath.erase(filepath.rfind('?'));
    }

    if (filepath.rfind('.') != std::string::npos)
    {
        mime = filepath.substr(filepath.rfind('.'));
    }
    decltype(Mime_map)::iterator it;

    if ((it = Mime_map.find(mime)) != Mime_map.end())
    {
        httpData->response_->setMime(it->second);
    }
    else
    {
        httpData->response_->setMime(Mime_map.find("default")->second);
    }
    httpData->response_->setFilePath(filepath);
}

HttpServer::FileState HttpServer::static_file(std::shared_ptr<HttpData> httpData, const char *basepath)
{
    struct stat file_stat;
    char file[strlen(basepath) + strlen(httpData->response_->filePath().c_str())+1];
    strcpy(file, basepath);
    strcat(file, httpData->response_->filePath().c_str());

    // 文件不存在
    if (httpData->response_->filePath() == "/" || stat(file, &file_stat) < 0)
    {
        httpData->response_->setMime(MimeType("text/html"));
        if (httpData->response_->filePath() == "/")
        {
            httpData->response_->setStatusCode(HttpResponse::k200Ok);
            httpData->response_->setStatusMsg("OK");
        }
        else
        {
            httpData->response_->setStatusCode(HttpResponse::k404NotFound);
            httpData->response_->setStatusMsg("Not Found");
        }
        return FIlE_NOT_FOUND;
    }
    // 不是普通文件或无访问权限
    if(!S_ISREG(file_stat.st_mode))
    {
        //
        httpData->response_->setMime(MimeType("text/html"));
        httpData->response_->setStatusCode(HttpResponse::k403forbiden);
        httpData->response_->setStatusMsg("ForBidden");
        //
        std::cout << "not normal file" << std::endl;
        return FILE_FORBIDDEN;
    }

    httpData->response_->setStatusCode(HttpResponse::k200Ok);
    httpData->response_->setStatusMsg("OK");
    httpData->response_->setFilePath(file);
//    std::cout << "文件存在 - ok" << std::endl;
    return FILE_OK;
}

void HttpServer::send(std::shared_ptr<HttpData> httpData, FileState fileState)
{
    char header[BUFFERSIZE];
    bzero(header, '\0');
    const char *internal_error = "Internal Error";
    struct stat file_stat;
    httpData->response_->appenBuffer(header);
    if (fileState == FIlE_NOT_FOUND)
    {
        //
        if (httpData->response_->filePath() == std::string("/"))
        {
            // 现在使用测试页面
            sprintf(header, "%sContent-length: %d\r\n\r\n", header, strlen(INDEX_PAGE));
            sprintf(header, "%s%s", header, INDEX_PAGE);
        }
        else
        {
            sprintf(header, "%sContent-length: %d\r\n\r\n", header, strlen(NOT_FOUND_PAGE));
            sprintf(header, "%s%s", header, NOT_FOUND_PAGE);
        }
        ::send(httpData->clientSocket_->fd, header, strlen(header), 0);
        return;
    }

    if (fileState == FILE_FORBIDDEN)
    {
        sprintf(header, "%sContent-length: %d\r\n\r\n", header, strlen(FORBIDDEN_PAGE));
        sprintf(header, "%s%s", header, FORBIDDEN_PAGE);
        ::send(httpData->clientSocket_->fd, header, strlen(header), 0);
        return;
    }
    // 获取文件状态
    if (stat(httpData->response_->filePath().c_str(), &file_stat) < 0)
    {
        sprintf(header, "%sContent-length: %d\r\n\r\n", header, strlen(internal_error));
        sprintf(header, "%s%s", header, internal_error);
        ::send(httpData->clientSocket_->fd, header, strlen(header), 0);
        return;
    }

    int filefd = ::open(httpData->response_->filePath().c_str(), O_RDONLY);
    // 内部错误
    if (filefd < 0)
    {
        std::cout << "打开文件失败" << std::endl;
        sprintf(header, "%sContent-length: %d\r\n\r\n", header, strlen(internal_error));
        sprintf(header, "%s%s", header, internal_error);
        ::send(httpData->clientSocket_->fd, header, strlen(header), 0);
        close(filefd);
        return;
    }

    sprintf(header,"%sContent-length: %d\r\n\r\n", header, file_stat.st_size);
    ::send(httpData->clientSocket_->fd, header, strlen(header), 0);
    void *mapbuf = mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, filefd, 0);
    ::send(httpData->clientSocket_->fd, mapbuf, file_stat.st_size, 0);
    munmap(mapbuf, file_stat.st_size);
    close(filefd);
    return;
err:
    sprintf(header, "%sContent-length: %d\r\n\r\n", header, strlen(internal_error));
    sprintf(header, "%s%s", header, internal_error);
    ::send(httpData->clientSocket_->fd, header, strlen(header), 0);
    return;
}

