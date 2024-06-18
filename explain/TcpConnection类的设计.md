# `TcpConnection` 类的设计

### `TcpConnection` 类

`TcpConnection.h` 头文件中 `TcpConnection` 类就是具体的连接实例类，也就是当 `Acceptor` 收到新连接后会调用上层 `TcpServer` 所注册的新连接建立回调 `TcpServer::newConnection` ，执行回调会把具体的 `socketfd` 用来建立一个 `TcpConnection` 的实例。当然，作为一个具体的连接实例类，除去具体的连接，还有连接对应的事件回调和 Buffer 缓冲区。 

```c++
/**
 * TcpServer  =>  Acceptor  =>  有一个新用户连接，通过 accrpt 函数拿到 connfd
 * =>  TcpConnection  设置回调  =>  channel  =>  Poller(epoll)  =>  Channel 回调操作
 */
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:

    TcpConnection(EventLoop *loop, 
                  const std::string &name, 
                  int sockfd, 
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    // 发送数据
    void send(const std::string &buf);

    // 关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb)
    {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback& cb)
    {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { 
        writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { 
        highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark;
    }

    void setCloseCallback(const CloseCallback& cb)
    { 
        closeCallback_ = cb; 
    }

    // 连接建立
    void connectEstablished();

    // 连接销毁
    void connectDestroyed();

private:

    enum StateE {
        kDisconnected,          // 断开连接
        kConnecting,            // 正在连接
        kConnected,             // 连接成功
        kDisconnecting,         // 正在断开连接
    };

    //
    void setState(StateE state) { state_ = state; } 

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();

    // 这里绝对不是 baseloop, 因为 TcpConnetion 都是在 subloop 里面管理的
    EventLoop *loop_;

    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    // 类似于 Acceptor 
    // acceptor 位于 mainLoop 中            TcpConnection 位于 subLoop 中
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    // 套接字对应的端口、ip信息
    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    // 相关回调操作
    ConnectionCallback connectionCallback_;                             // 新连接回调
    MessageCallback messageCallback_;                                   // 读写消息的回调
    WriteCompleteCallback writeCompleteCallback_;                       // 消息发送时的回调
    HighWaterMarkCallback highWaterMarkCallback_;                       // 水位线回调？？？
    CloseCallback closeCallback_;                                       // 关闭连接回调

    size_t highWaterMark_;                                              // 警告水位线

    // 数据缓冲区
    Buffer inputBuffer_;                                                // 接受数据的缓冲区
    Buffer outputBuffer_;                                               // 发送数据的缓冲区

};
```

`TcpConnection.cc` 源文件主要函数解读：

- `TcpConnection(EventLoop *loop,`  `const std::string &nameArg,` `int sockfd,`  `const InetAddress& localAddr,` `const InetAddress& peerAddr)` 构造函数

构造函数主要干两件事，一是初始化类成员，二是绑定具体的回调函数。



- `void shutdown()` 函数

该函数主要用来关闭连接，通过 `bind` 向下层绑定函数 `shutdownInLoop` ，并最终在 `EventLoop` 中执行。

- `void shutdownInLoop()` 函数

该函数最终会在 `EventLoop` 中执行，用于关闭写端。由于内核中的 `bufeer` 还有可读数据，所以在此仅关闭写端。



- `void send(const std::string &buf)` 函数

该函数主要用来服务端返回发送给客户端数据，通过执行 `sendInLoop` 进行发送。

- `void sendInLoop(const void* data, size_t len)` 函数

主要还是通过调用 `write` 函数向内核缓冲区写入数据。



**回调函数**

以下回调函数该类具体实例对象所对应的 `socketfd` 对应的回调函数，会往下层注册到 `Channel` 中，但具体实现依然是由 `EventLoop` 类的 `loop` 事件循环中进行调用。

- `void handleRead(Timestamp receiveTime)` 函数

该函数是发生读事件的具体回调，主要执行应用层 `Buffer` 缓冲区的 `readFd` 方法，将内核中的数据通过 `read (readv)` 读到应用层输入缓冲区，然后执行具体的 **读事件回调**。

- `void handleWrite()` 函数

该函数是发生写事件的具体回调，主要执行应用层 Buffer 缓冲区的 writeFd 方法，将应用层缓冲区通过 write 函数写入内核缓冲区中，内核再根据协议内容发送到对端。然后执行具体的 **写事件回调**。

- `void handleClose()` 函数

该函数是发生关闭事件的具体回调，主要执行 **用户层连接关闭的回调** 和上层注册的连接关闭的回调，也就是执行 `TcpServer::removeConnection` 回调方法。

- `void handleError()` 函数

该函数是发生关闭事件的具体回调，主要作用是当套接字发生错误时，该函数会被调用，通过 `getsockopt` 获取具体的错误码，然后使用 `LOG_ERROR` 宏记录错误信息，便于调试和错误处理。这有助于维护和监控 TCP 连接的健康状态，及时发现并处理潜在的问题。

> 注：上述标黑所对应的事件回调均是用户层面所向下注册的回调，也就是示例代码 `testserver.cc` 中所写的 `onMessage` 、`onConnection`  等函数。



**上层调用函数**

实际上，我们 `TcpConnection` 这个类是由上层 `TcpServer` 来进行具体的实例化，也就是说 `conn` 连接和断开是由上层处理。所以如果我们需要在断开和连接时有对应的操作，可以提供共有的方法供上层调用。

- `void connectEstablished()` 函数

该函数为连接建立时上层执行的函数，主要是向 `epoll` 上注册 `channel` 感兴趣的事件，并且执行 `connectionCallback_` 新连接建立回调。

- `void connectDestroyed()` 函数

该函数为连接关闭时上层执行的函数，主要是把  `epoll` 上取消关注 `channel` 感兴趣的事件，并且执行 `connectionCallback_` 关闭接建立回调。

```c++
static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, 
                  const std::string &nameArg, 
                  int sockfd, 
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64 * 1024 * 1024)  // 64 M 
{
    // 给 Channel 设置相应的回调函数，poller 给 channel 通知感兴趣的事件，Channel 会自动调用他的回调函数

    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d \n", name_.c_str(), sockfd);
    
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string &buf)
{
    // 当属于正在连接的状态
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

// 发送数据， 应用写的快，内核发送数据慢，需要把待发送数据写入缓冲区，而且设置了水位回调
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len; 
    bool faultError = false;

    // 之前调用过该 connection 的 shutdown ， 不能在进行发送了
    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return ; 
    }

    // 表示 channel_ 第一次开始写数据，而且缓冲区没有待发送的数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                // 既然一次性数据发送完成，就不用再给 channel 设置 epollout 事件了
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else  // 出错
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET)
                {   
                    faultError = true;
                }
            }
        }
    }

    // 说明一次性并没有发送完数据，剩余数据需要保存到缓冲区中，且需要改channel注册写事件
    if (!faultError && remaining > 0) 
    {
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            // TODO
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char *)data + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting(); // 这里一定要注册channel的写事件 否则poller不会给channel通知epollout
        }
    }
}

// 关闭连接
void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this)
        );
    }
}

void TcpConnection::shutdownInLoop()
{
    // 说明当前 outputBuffer 中的数据已经全部发送完
    if (!channel_->isWriting())
    {
        // 关闭写端
        socket_->shutdownWrite();
    }
}


void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    
    
    if (n > 0) 
    {
        // 已经建立连接的用户发送可读事件，调用用户传入的回调操作 onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } 
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        
        // 正确读取数据
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            
            // 说明buffer可读数据都被TcpConnection读取完毕并写入给了客户端
            // 此时就可以关闭连接，否则还需继续提醒写事件
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    // 唤醒 loop_ 对应的 thread 线程，执行回调
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else{
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else{
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n",channel_->fd());
    }
}   

void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d", channel_->fd(), (int)state_);
    setState(kDisconnected);                                // 设置状态为关闭连接状态
    channel_->disableAll();                                 // 注销Channel所有感兴趣事件

    TcpConnectionPtr connPtr(shared_from_this());

    // 执行连接关闭的回调
    connectionCallback_(connPtr);

    // 执行关闭连接的回调 执行的是 TcpServer::removeConnection 回调方法
    closeCallback_(connPtr);
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else{
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}


// 连接建立
void TcpConnection::connectEstablished()
{
    // 连接成功
    setState(kConnected);
    channel_->tie(shared_from_this());

    // 设置该 channel 关注读事件
    channel_->enableReading();

    // 新建连接，执行回调
    connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);

        // 把 channel 的所有感兴趣的事件，从 poller 中 del 掉
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }

    // 从 Poller 中删除 channel
    channel_->remove();
}
```

