#include <functional>
#include <string>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

#include "TcpConnection.h"
#include "asLogger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"


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

