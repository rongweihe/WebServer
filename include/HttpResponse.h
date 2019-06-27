/********************************************************
*@file  封装 HttpResponse 类 浏览器返回结果
*@Author rongweihe
*@Data 2019/03/31
*********************************************************/
#ifndef HTTPRESPONSE_H_INCLUDED
#define HTTPRESPONSE_H_INCLUDED

#include "HttpRequest.h"

#include <string>
#include <unordered_map>
#include <memory>


struct MimeType
{
    MimeType(const std::string &str) : type(str) {};
    MimeType(const char *str) : type(str) {};
    std::string type;
};

extern std::unordered_map<std::string, MimeType> Mime_map;

class HttpResponse
{
public:
    enum HttpStatusCode
    {
        Unknow,
        k200Ok = 200,
        k403Forbiden = 403,
        k404NotFound = 404
    };
    explicit HttpResponse(bool mkeep = true)
        : mStatusCode(Unknow), keep_alive_(mkeep), mMime("text/html"), kmBody(nullptr),
          mVersion(HttpRequest::HTTP_11) {}

    void setStatusCode(HttpStatusCode code)
    {
        mStatusCode = code;
    }

    void setBody(const char *buf)
    {
        kmBody = buf;
    }

    void setContentLength(int len)
    {
        mContentLength = len;
    }

    void setVersion(const HttpRequest::HTTP_VERSION &version)
    {
        mVersion = version;
    }

    void setStatusMsg(const std::string &msg)
    {
        mStatusMsg = msg;
    }

    void setFilePath(const std::string &path)
    {
        mFilePath = path;
    }

    void setMime(const MimeType &mime)
    {
        mMime = mime;
    }

    void setKeepAlive(bool isalive)
    {
        keep_alive_ = isalive;
    }

    void addHeader(const std::string &key, const std::string &value)
    {
        mHeaders[key] = value;
    }

    bool keep_alive() const
    {
        return keep_alive_;
    }

    const HttpRequest::HTTP_VERSION version() const
    {
        return m_version_;
    }

    const std::string &filePath() const
    {
        return mFilePath;
    }

    HttpStatusCode statusCode() const
    {
        return m_status_code_;
    }

    const std::string &statusMsg() const
    {
        return mStatusMsg;
    }

    void appenBuffer(char *) const;

    ~HttpResponse()
    {
        if (kmBody != nullptr)
            delete[] kmBody;
    }
private:
    ///类成员以下划线结尾
    HttpStatusCode m_status_code_;
    HttpRequest::HTTP_VERSION m_version_;
    std::string mStatusMsg;
    bool keep_alive_;
    MimeType mMime;
    const char *kmBody;
    int mContentLength;
    std::string mFilePath;
    std::unordered_map<std::string, std::string> mHeaders;
};
#endif // HTTPRESPONSE_H_INCLUDED
