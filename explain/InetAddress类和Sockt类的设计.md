# `InetAddress` 类和 `Socket` 类的设计

### 一个 `socket` 创建的顺序

```c++
// 创建 sockaddr_in 结构体
struct sockaddr_in serv_addr;
bzero(&serv_addr, sizeof(serv_addr));

// 设置地址族、IP地址和端口
serv_addr.sin_family = AF_INET;
serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
serv_addr.sin_port = htons(8888);


// 然后将 socket 地址与文件描述符绑定
bind(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr));

// 使用 listen 函数监听这个 socket 端口
listen(sockfd, SOMAXCONN);

// accept 函数（需要创建 sockaddr_in 存放客户端的socket地址信息）
struct sockaddr_in clnt_addr;
socklen_t clnt_addr_len = sizeof(clnt_addr);
bzero(&clnt_addr, sizeof(clnt_addr));
int clnt_sockfd = accept(sockfd, (sockaddr*)&clnt_addr, &clnt_addr_len);
```

而在下述中 `InetAddress` 类主要就是对创建 `sockaddr_in` 结构体后设置服务端地址族、IP地址和端口进行的一个封装。而 `Socket` 类是对 `bind()` 、`listen()` 、`accept()` 三个函数的封装。



### `InetAddress` 类

`InetAddress.h` 头文件中 `InetAddress` 类就是将服务器的地址信息和端口信息进行封装，同时提供一些格式化输出的函数。

```c++
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
```

`InetAddress.cc` 源文件主要函数解读：

- `InetAddress(uint16_t port, std::string ip)` 构造函数

构造函数默认 `port` 为 0，默认 `ip` 为 `“127.0.0.1”`。主要操作就是对网络编程中，对 `sockaddr_in` 这个结构体进行协议族、端口、`ip` 地址的一个赋值。

```c++
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
```



### `Socket` 类

`Socket.h` 头文件中 `Socket` 类就是是对 `bind()` 、`listen()` 、`accept()` 三个函数的封装。

```c++
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {}
    ~Socket();

    int fd() const { return sockfd_; };
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress *peeraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);


private:
    const int sockfd_;
};
```

`Socket.cc` 源文件主要函数解读：

- `void shutdownWrite()` 函数

该函数是对 `sockfd_` 进行半关闭，代码中关闭了输出流，套接字无法发送数据，但是还可以读数据。

> `SHUT_RD`：断开输入流。套接字无法接收数据（即使输入缓冲区收到数据也被抹去），无法调用输入相关函数。
>
> `SHUT_WR`：断开输出流。套接字无法发送数据，但如果输出缓冲区中还有未传输的数据，则将传递到目标主机。
>
> `SHUT_RDWR`：同时断开 `I/O` 流。相当于分两次调用 `shutdown()`，其中一次以 `SHUT_RD` 为参数，另一次以 `SHUT_WR` 为参数。

- `void setTcpNoDelay(bool on)` 函数

该函数用来设置或清除 `TCP_NODELAY` 选项。

> `TCP_NODELAY`：禁用 `Nagle` 算法。`Nagle` 算法通过减少小分组的发送来增加网络利用率。禁用该算法意味着数据包会立即发送，而不会等待更多的数据。
>
> **用途**：在低延迟通信中（如实时应用程序）非常有用。

- `void setReuseAddr(bool on)` 函数

该函数用来设置或清除 `SO_REUSEADDR` 选项。

> **`SO_REUSEADDR`**：允许在本地地址（IP 地址和端口）已被占用的情况下绑定套接字。这通常用于服务器重新启动时，可以立即重新绑定端口而不需要等待上一个连接完全关闭。
>
> **用途**：提高服务器的可用性和灵活性，特别是在开发和调试阶段。

- `void  setReuseAddr(bool on)` 函数

该函数用来设置或清除 `SO_REUSEPORT` 选项。

> **`SO_REUSEPORT`**：允许多个套接字绑定到同一个 IP 地址和端口号。不同的套接字会接收同一个 IP/端口的不同数据包。
>
> **用途**：在多进程或多线程服务器中，多个进程或线程可以共享同一个端口，提高负载均衡能力和并发处理能力。

- `void setKeepAlive(bool on)` 函数

该函数用来设置或清除 `SO_KEEPALIVE` 选项。

> **`SO_KEEPALIVE`**：开启 TCP 保活机制。保活机制通过周期性发送探测包来检测连接是否仍然存在。如果对方没有响应，连接会被关闭。
>
> **用途**：在长时间没有数据交换的情况下，确保连接的存活状态。这对于需要长期保持连接的应用（如远程登录、持久连接等）非常重要。



```c++
Socket::~Socket()
{
    close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr)
{
    if (0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL("bind sockfd:%d fail \n", sockfd_);
    }
}

void Socket::listen()
{
    if (0 != ::listen(sockfd_, 1024))
    {
        LOG_FATAL("listen sockfd:%d fail \n", sockfd_);
    }
}

int Socket::accept(InetAddress *peeraddr)
{
    sockaddr_in addr;
    socklen_t len = sizeof addr;
    bzero(&addr, sizeof addr);
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0)
    {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_ERROR("shutdownWrite error");
    }
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}   

```

