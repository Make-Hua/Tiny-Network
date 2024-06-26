#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#include "Acceptor.h"
#include "asLogger.h"
#include "InetAddress.h"


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

// socketfd 有事件发生了，也就是有新用户连接了
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