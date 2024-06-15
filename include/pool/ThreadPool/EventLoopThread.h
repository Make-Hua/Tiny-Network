#pragma once

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

#include "noncopyable.h"
#include "EventLoop.h"
#include "Thread.h"

// 实现 one loop thread 模型(一个 loop 一个线程)
class EventLoopThread : noncopyable
{

public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(), const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();

private:

    void threadFunc();

    EventLoop *loop_;                       // 当前线程对应的 loop
    bool exiting_;                          // 
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;           // 线程初始化的回调函数
};

