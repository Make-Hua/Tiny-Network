#include "HttpResponse.h"
#include "Buffer.h"

#include <stdio.h>
#include <string.h>

/* 重构
        《=========================》
*/



// 将响应内容写入缓冲区打包好
void HttpResponse::appendToBuffer(Buffer* output) const
{
    // 响应行
    char buf[32];
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d ", statusCode_);
    output->append(buf, strlen(buf));
    output->append(statusMessage_.c_str(), statusMessage_.size());
    output->append("\r\n", 2);

    // 连接状态
    if (closeConnection_)
    {
        output->append("Connection: close\r\n", 19);
    }
    else
    {
        snprintf(buf, sizeof(buf), "Content-Length: %zd\r\n", body_.size());
        output->append(buf, strlen(buf));
        output->append("Connection: Keep-Alive\r\n", 24);
    }

    // 头部字段
    for (const auto& header : headers_)
    {
        output->append(header.first.c_str(), header.first.size());
        output->append(": ", 2);
        output->append(header.second.c_str(), header.second.size());
        output->append("\r\n", 2);
    }

    // 空行
    output->append("\r\n", 2);

    // 加入响应体
    output->append(body_.c_str(), body_.size());
}