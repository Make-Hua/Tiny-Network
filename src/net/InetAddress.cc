#include "InetAddress.h"

#include <strings.h>
#include <string.h>

// 封装
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    ::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}
    
// get ip 地址
std::string InetAddress::toIp() const 
{
    // addr_
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    return buf;
}

// get ip+port ip地址+端口
std::string InetAddress::toIpPort() const
{
    // addr_
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u", port);
    return buf;
}

// get port 端口
uint16_t InetAddress::toPort() const
{
    return ::ntohs(addr_.sin_port);
}