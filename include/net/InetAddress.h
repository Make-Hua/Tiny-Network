#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

// 处理套接字地址和端口等信息，同时可以获得本地套接字的端口、ip地址等信息
class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr)
    {}

    // get 操作
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const { return &addr_; }
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

private:
    sockaddr_in addr_;
};