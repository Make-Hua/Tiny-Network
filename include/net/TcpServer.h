#pragma once

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

#include "noncopyable.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

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

    // 提供给Http用
    EventLoop* getLoop() const { return loop_; }
    const std::string name() { return name_; }
    const std::string ipPort() { return ipPort_; }

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