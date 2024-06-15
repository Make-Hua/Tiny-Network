#include "EventLoopThread.h"



EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb)
{

}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop()
{
    // 启动一个底层的新线程
    thread_.start();

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (nullptr == loop_)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }  
    return loop;
}

// 下面这个方法，是在单独的一个新线程执行的
void EventLoopThread::threadFunc()
{
    // 创建一个独立的 Eventloop ， 和上面的线程是一一对应的
    EventLoop loop;
    
    // 如果存在 线程创建回调函数则执行
    if (callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    // EventLoop loop -> Poller poll 底层开启了 poller 的 poll()
    loop.loop();

    // 上述的 loop.loop() 是死循环，当需要关闭 EventLoop 时则执行下述操作
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;

}