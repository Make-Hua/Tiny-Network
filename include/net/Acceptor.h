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

    // Acceptor 用的就是用户定义的 baseloop(mainloop)
    EventLoop *loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;

};