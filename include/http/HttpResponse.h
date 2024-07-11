#pragma once

#include <unordered_map>
#include <string>


class Buffer;

/*
状态行（Status Line）：
    HTTP 版本：与请求消息中的版本相匹配。
    状态码：三位数，表示请求的处理结果，如 200 表示成功，404 表示未找到资源。
    状态信息：状态码的简短描述。
    状态行的格式示例：HTTP/1.1 200 OK
响应头（Response Headers）：
    包含了服务器环境信息、响应体的大小、服务器支持的压缩类型等。
    常见的响应头包括Content-Type、Content-Length、Server、Set-Cookie等。
空行：
    响应头和响应体之间的分隔符，表示响应头的结束。
响应体（可选）：
    包含服务器返回的数据，如请求的网页内容、图片、JSON数据等。


    示例：
HTTP/1.1 200 OK


Date: Wed, 18 Apr 2024 12:00:00 GMT
Server: Apache/2.4.1 (Unix)
Last-Modified: Wed, 18 Apr 2024 11:00:00 GMT
Content-Length: 12345
Content-Type: text/html; charset=UTF-8


<!DOCTYPE html>
<html>
<head>
    <title>Example Page</title>
</head>
<body>
    <h1>Hello, World!</h1>
    <!-- The rest of the HTML content -->
</body>
</html>

*/


class HttpResponse
{
public:
    // HTTP响应状态码
    enum HttpStatusCode
    {
        kUnknown,                   // 未知状态码
        k200Ok = 200,               // 成功
        k301MovedPermanently = 301, // 永久重定向
        k400BadRequest = 400,       // 错误请求
        k404NotFound = 404,         // 未找到
    };  

    explicit HttpResponse(bool close)
        : statusCode_(kUnknown)
        , closeConnection_(close)
    {}   

    // 设置响应状态码
    void setStatusCode(HttpStatusCode code)
    { statusCode_ = code; } 

    // 设置响应状态消息
    void setStatusMessage(const std::string& message)
    { statusMessage_ = message; }   

    // 设置是否关闭连接
    void setCloseConnection(bool on)
    { closeConnection_ = on; }  

    // // 设置响应体
    void setBody(const std::string& body)
    { body_ = body; }   

    // 获取连接关闭标志
    bool closeConnection() const
    { return closeConnection_; }  

    // 设置Content-Type头部字段
    void setContentType(const std::string& contentType)
    { 
        addHeader("Content-Type", contentType); 
    } 

     // 添加一个头部字段
    void addHeader(const std::string& key, const std::string& value)
    { 
        headers_[key] = value;
    }  

    // 将响应内容写入缓冲区打包好
    void appendToBuffer(Buffer* output) const;

private:
    std::unordered_map<std::string, std::string> headers_;          // 头部字段集合
    HttpStatusCode statusCode_;                                     // 响应状态码
    std::string statusMessage_;                                     // 响应状态消息
    bool closeConnection_;                                          // 是否关闭连接
    std::string body_;                                              // 响应体
};


