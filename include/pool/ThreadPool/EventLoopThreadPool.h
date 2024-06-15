#pragma once

#include <functional>
#include <string>
#include <memory>
#include <vector>

#include "noncopyable.h"

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    // 
    void start(const ThreadInitCallback &cb = ThreadInitCallback());


    // 如果工作在多线程中，baseLoop_默认以轮询的方式分配 channel 给 subloop
    EventLoop* getNextLoop();

    // 
    std::vector<EventLoop*> getAllLoops();

    // 是否运行
    bool started() const { return started_; }
    
    // 获取 name_
    const std::string name() const { return name_; }

private:
    EventLoop *baseLoop_;										// mainLoop(主Reactor)
    std::string name_;											// 名称
    bool started_;												// 是否启动线程池
    int numThreads_;											// 总共有几个线程
    int next_;													// 轮询的下标
    std::vector<std::unique_ptr<EventLoopThread>> threads_;		// 	
    std::vector<EventLoop*> loops_;
};