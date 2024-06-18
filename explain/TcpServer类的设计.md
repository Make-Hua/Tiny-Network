# `TcpServer` 类的设计

### `TcpServer` 类

`TcpServer.h` 头文件中 `TcpServer` 类就是提供给用户使用的类，同时能能够让我们所有模块之间进行交互。

```c++
// 对外服务器编程使用的类
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option
    {
        kNoReusePort,
        kReusePost,
    };

    TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string nameArg, Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitcallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectioncallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessagecallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompletecallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }


    // 设置底层 subloop 的个数
    void setThreadNum(int numThreads);

    // 开启服务监听
    void start();

private:

    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_;
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;                                 // 运行在 mainLoop 主要是监听新的连接事件
    std::shared_ptr<EventLoopThreadPool> threadPool_;
    
    ConnectionCallback connectionCallback_;                             // 新连接回调
    MessageCallback messageCallback_;                                   // 读写消息的回调
    WriteCompleteCallback writeCompleteCallback_;                       // 消息发送时的回调

    ThreadInitCallback threadInitCallback_;                             // loop 线程初始化的回调
    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_;                                         // 保存所有连接
};
```

`TcpServer.cc` 源文件主要函数解读：

- `TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string nameArg, Option option)` 构造函数

构造函数主要是对该类的成员进行初始化，同时给成员 `acceptor_` 中注册 `newConnection` 回调，以便 `acceptor_` 中进行调用。

- `~TcpServer()` 析构函数

析构函数需要将成员对象中的 `connections_` 中所有的连接进行释放，通过执行 `TcpConnection` 向上层 `TcpServer` 提供的接口函数，进行释放等相关操作。



- `void setThreadNum(int numThreads)` 函数

该函数向下调用线程池的 `setThreadNum` 函数进行设置线程数量。

- `void start()` 函数

该函数会调用线程池的 `start` 函数并传入 线程启动回调；同时会启动成员 `acceptor_` ，即调用 `listen` 开启监听。

> 注：线程池的 `start` 函数具体执行流程请看线程池设计实现。



- `void newConnection(int sockfd, const InetAddress &peerAddr)` 函数

该函数主要是在新连接建立的时候会进行调用，是通过向下注册到 `Acceptor` 实例对象中，然后再具体的连接事件回调 `Acceptor::handleRead()` 中调用。

具体流程为：当有新连接建立的时候， Acceptor 会返回一个连接对应的 `connfd` ，然后此时会调用该函数，再函数内，首先会通过轮询算法选取一个 `subloop` ，然后创建具体的 `TcpConnection` 实例，并且将用户注册的连接、读事件、写事件、关闭事件的回调向 `TcpConnection` 实例中注册，最后通调用 `connectEstablished`。



- `void removeConnection(const TcpConnectionPtr &conn)` 函数

该函数向下传递 `removeConnectionInLoop` 函数并再 `EventLoop` 中执行。

- `void removeConnectionInLoop(const TcpConnectionPtr &conn)` 函数

该函数主要是断开连接执行函数，主要是调用 `TcpConnection` 对外提供的 `connectDestroyed` 进行对 `conn` 连接销毁的操作。

```c++
static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}


TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string nameArg, Option option)
    : loop_(loop)
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePost))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
    , started_(0)
{
    // 当有新用户连接时，会执行 TcpServer::newConnection 回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
    
}

TcpServer::~TcpServer()
{
    for (auto &it : connections_)
    {
        TcpConnectionPtr conn(it.second);
        it.second.reset();

        // 销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)
        );
    }
}

// 设置底层 subloop 的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

// 开启服务监听
void TcpServer::start()
{
    // 防止一个对象被启动多次
    if (started_++ == 0) 
    {
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    // 轮询算法  选一个 subloop，来管理channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过 sockfd 获取对应主机的 ip 和 prot
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }

    InetAddress localAddr(local);

    // 根据连接成功的 sockfd，创建 TcpConnection 对象
    TcpConnectionPtr conn(new TcpConnection(
                               ioLoop,
                               connName,
                               sockfd,
                               localAddr,
                               peerAddr
                        ));
    connections_[connName] = conn;

    // 下面的回调都是用户设置给 TcpServer => TcpConnection => Channel => Poller
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了关闭连接的回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
    );

    // 直接调用 connectEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}


void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn)
    );
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s \n", name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn)
    );
}

```

