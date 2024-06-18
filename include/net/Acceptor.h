#pragma once

#include <functional>

#include "noncopyable.h"
#include "Channel.h"
#include "Socket.h"

class EventLoop;
class InetAddress;

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
    bool listenning_;

};