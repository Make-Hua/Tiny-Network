#pragma once

#include "HttpRequest.h"

class Buffer;


// 用于解析 Http 请求的
class HttpContext
{
public:
    // HTTP请求状态
    enum HttpRequestParseState
    {
        kExpectRequestLine, // 解析请求行状态
        kExpectHeaders,     // 解析请求头部状态
        kExpectBody,        // 解析请求体状态
        kGotAll,            // 解析完毕状态
    };

    HttpContext()
        : state_(kExpectRequestLine)
    {}

    // 解析请求
    bool parseRequest(Buffer* buf, Timestamp receiveTime);

    // 判断是否解析完毕
    bool gotAll() const { return state_ == kGotAll; }

    // 重置HttpContext状态，异常安全
    void reset()
    {
        state_ = kExpectRequestLine;
        /**
         * 构造一个临时空HttpRequest对象，和当前的成员HttpRequest对象交换置空
         * 然后临时对象析构
         */
        HttpRequest dummy;
        request_.swap(dummy);
    }

    // 获取请求对象（常量版本）
    const HttpRequest& request() const { return request_; }

    // 获取请求对象（非常量版本）
    HttpRequest& request() { return request_; }

private:
    // 处理请求行
    bool processRequestLine(const char *begin, const char *end);

    HttpRequestParseState state_;                   // 当前解析状态
    HttpRequest request_;                           // HTTP 请求对象
};



