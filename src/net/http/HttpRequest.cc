

#include "HttpRequest.h"


HttpRequest::HttpRequest()
    : method_(kInvalid)
    , version_(kUnknown)
{}

void HttpRequest::setVersion(Version v)
{
    version_ = v;
}

void HttpRequest::setPath(const char *start, const char *end)
{
    path_.assign(start, end);
}

void HttpRequest::setQuery(const char *start, const char *end) 
{
    query_.assign(start, end);
}

void HttpRequest::setReceiveTime(Timestamp t) 
{ 
    receiveTime_ = t; 
}

bool HttpRequest::setMethod(const char* start, const char* end) 
{
    std::string s(start, end);
    
    if (s == "GET") method_ = kGet;
    else if (s == "POST") method_ = kPost;
    else if (s == "HEAD") method_ = kHead;
    else if (s == "PUT") method_ = kPut;
    else if (s == "DELETE") method_ = kDelete;
    else method_ = kInvalid;

    // 判断 method_ 是否合法
    return method_ != kInvalid;
}


// 回去具体请求方法字符串
const char* HttpRequest::methodString() const
{
    const char* result = "UNKNOWN";
    switch(method_)
    {
    case kGet:
        result = "GET";
        break;
    case kPost:
        result = "POST";
        break;
    case kHead:
        result = "HEAD";
        break;
    case kPut:
        result = "PUT";
        break;
    case kDelete:
        result = "DELETE";
        break;
    default:
        break;
    }
    return result;
}

/*
请求行（Request Line）：
    方法：如 GET、POST、PUT、DELETE等，指定要执行的操作。
    请求 URI（统一资源标识符）：请求的资源路径，通常包括主机名、端口号（如果非默认）、路径和查询字符串。
    HTTP 版本：如 HTTP/1.1 或 HTTP/2。
    请求行的格式示例：GET /index.html HTTP/1.1

请求头（Request Headers）：
    包含了客户端环境信息、请求体的大小（如果有）、客户端支持的压缩类型等。
    常见的请求头包括Host、User-Agent、Accept、Accept-Encoding、Content-Length等。
空行：
    请求头和请求体之间的分隔符，表示请求头的结束。
请求体（可选）：
    在某些类型的HTTP请求（如 POST 和 PUT）中，请求体包含要发送给服务器的数据。
*/
// 例如
// GET /index.html HTTP/1.1
// Host: www.example.com
// User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:91.0) Gecko/20100101 Firefox/91.0
// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8
// Accept-Encoding: gzip, deflate
// Connection: keep-alive

// 添加一个 HTTP 请求的头部字段
void HttpRequest::addHeader(const char *start, const char *colon, const char *end)
{
    std::string field(start, colon);
    ++colon;

    // 过滤空格
    while (colon < end && isspace(*colon))
    {
        ++colon;
    }

    std::string value(colon, end);
    
    // value丢掉后面的空格，通过重新截断大小设置
    while (!value.empty() && isspace(value[value.size() - 1]))
    {
        value.resize(value.size() - 1);
    }

    headers_[field] = value;
}

// 获取请求头部的对应值
std::string HttpRequest::getHeader(const std::string &field) const
{
    std::string result;
    auto it = headers_.find(field);
    if (it != headers_.end())
    {
        result = it->second;
    }
    return result;
}


// 方便操作
void HttpRequest::swap(HttpRequest &rhs) 
{
    std::swap(method_, rhs.method_);
    std::swap(version_, rhs.version_);
    path_.swap(rhs.path_);
    query_.swap(rhs.query_);
    std::swap(receiveTime_, rhs.receiveTime_);
    headers_.swap(rhs.headers_);
}



