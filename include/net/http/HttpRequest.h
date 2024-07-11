#pragma once

#include <unordered_map>
#include <string>

#include "noncopyable.h"
#include "Timestamp.h"

class HttpRequest
{
public:
    // 枚举类型 Method 用于表示 HTTP 请求方法。
    enum Method { 
        kInvalid, 
        kGet, 
        kPost, 
        kHead, 
        kPut, 
        kDelete 
    };

    // 枚举类型 Version 用于表示 HTTP 协议的版本。
    enum Version {
        kUnknown, 
        kHttp10, 
        kHttp11 
    };

    HttpRequest();

    // 修改函数
    void setVersion(Version v);
    bool setMethod(const char* start, const char* end);
    void setPath(const char *start, const char *end);
    void setQuery(const char *start, const char *end);
    void setReceiveTime(Timestamp t);

    // 获取 method_ 函数
    Version version() const  { return version_; }
    Method method() const { return method_; }
    const std::string& path() const { return path_; }
    Timestamp receiveTime() const { return receiveTime_; }
    const std::unordered_map<std::string, std::string>& headers() const { return headers_; }


    // 回去具体请求方法字符串
    const char* methodString() const; 

    // 添加一个 HTTP 请求的头部字段
    void addHeader(const char *start, const char *colon, const char *end);

    // 获取请求头部的对应值
    std::string getHeader(const std::string &field) const;

    // 方便操作
    void swap(HttpRequest &rhs);


private:
    Method method_;                                             // 请求方法
    Version version_;                                           // 协议版本号
    std::string path_;                                          // 请求路径
    std::string query_;                                         // 询问参数
    Timestamp receiveTime_;                                     // 请求时间
    std::unordered_map<std::string, std::string> headers_;      // 请求头部列表
};



