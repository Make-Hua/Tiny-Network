# `Acceptor` 类的设计

### `Acceptor` 类前言

通过服务器开发流程我们可以发现，当我们通过任意一种方式监听到有具体事件发送，对于每一个事件，不管提供什么样的服务，首先需要做的事都是调用 `accept()` 函数接受这个 TCP 连接，然后将 `socketfd` 文件描述符添加到 `epoll`。当这个 IO 口有事件发生的时候，再对此 TCP 连接提供相应的服务。

`Acceptor`类最主要的三个特点：

- 类存在于事件驱动 `EventLoop` 类中，也就是 `Reactor` 模式的 `mainReactor`
- 类中的 `acceptSocket_` 对应的就是服务器中进行监听的 `socketfd`，每一个 `Acceptor` 实例对象对应一个 `socketfd`
- 既然拥有独立的 `socketfd`  ，说明这个类也通过一个独有的 `Channel` 负责分发到 `epoll`，该 `Channel` 对应的 `socketfd`  所监听到连接事件后会调用 `handleRead()` 函数，新建一个 TCP 连接，同时如若上层 `TcpServer` 对新建连接注册过相应回调则会进行执行 `newConnectionCallback_`。

为了实现上述回调这一设计，我们可以用两种方式：

- 使用传统的虚类、虚函数来设计一个接口

- C++11的特性：`std::function`、`std::bind`、右值引用、`std::move` 等实现函数回调

而虚函数使用起来比较繁琐，程序的可读性也不够清晰明朗，而 `std::function`、`std::bind` 等新标准的出现可以完全替代虚函数，所以本教程采用第二种方式。



### `Acceptor` 类

`Acceptor.h` 头文件中 `Acceptor` 类主要用于在网络编程中处理新的连接请求。`Acceptor` 类中会有对应的 `acceptSocket_` ，该文件描述符会注册在 `mainLoop` 对应的 `epoll` 中，负责监听新的连接并接受这些连接，将新连接的文件描述符交给上层处理。

```c++
class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();    

    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = cb;
    }

    bool listenning() const { return listenning_; }
    void listen();

private:
    void handleRead();
    
    EventLoop *loop_;                                       // Acceptor 用的就是用户自己定义的主事件循环 baseloop(mainloop)
    Socket acceptSocket_;                                   // 监听用的 fd 
    Channel acceptChannel_;                                 // 该 fd 对应的 Channel
    NewConnectionCallback newConnectionCallback_;           // 建立连接的回调函数
    bool listenning_;										// 判断是否开启 listen

};
```



`Acceptor.cc` 源文件主要函数解读：

- `static int createNonblocking()` 全局静态函数

该函数用来给上层 `TcpServer` 创建 `Acceptor` 对象时调用，通过 `socket` 创建具体的 `socketfd`。

- `Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)` 构造函数

构造函数初始化类中成员，主要需要注册具体的连接事件发生的回调。

- `~Acceptor()` 析构函数

析构函数用来关闭该类对应的 `socketfd` ，具体的 `colse` 在具体的 `Socket` 类中进行析构，在 `Acceptor` 主要是要把 `socketfd` 所注册的 `epoll` 进行取消关注。

- `void handleRead()` 函数

该函数为 `socketfd` 发生读事件时执行的函数，主要通过 `accept` 获取客户端发起的对应连接，同时如果上层注册了相应的读事件回调，则会执行。





```c++
// 因为是上层 TcpServer 创建 Acceptor 对象时调用的，所以需要写成静态函数
static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0) 
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);

    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{   
    // 关闭对应的文件描述符 （下述操作时关闭文件描述符的前置操作，具体关闭在 Socket 中）
    acceptChannel_.disableAll();            // 禁用读写事件
    acceptChannel_.remove();                // 将该文件描述符从 epoll 删除
} 

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();

    // 把当前 acceptChannel_ 注册到 poller 上
    acceptChannel_.enableReading();    
}

// listenfd 有事件发生了，也就是有新用户连接了
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    
    if (connfd >= 0)
    {
        if (newConnectionCallback_)
        {
            // 轮询找到 subloop，然后唤醒， 分发当前用户的新客户端的 Channel
            newConnectionCallback_(connfd, peerAddr);
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept errno: %d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}
```

