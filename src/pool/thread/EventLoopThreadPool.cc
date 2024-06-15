#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"


EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0)
{}

EventLoopThreadPool::~EventLoopThreadPool() {}

// 启动线程池
void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;

    for (int i = 0; i < numThreads_; ++i) 
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "make-%s-%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        
        // 在 EventLoopThread 创建线程，绑定一个新的 EventLoop，并返回该 loop 的地址
        loops_.push_back(t->startLoop());
    }

    // 服务端只有一个线程，运行着 baseloop
    if (numThreads_ == 0 && cb)
    {  
        cb(baseLoop_);
    }
}


// 如果工作在多线程中，baseLoop_默认以轮询的方式分配 channel 给 subloop
EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseLoop_;

    if (!loops_.empty())
    {
        loop = loops_[next_++];
        next_ = (next_ >= loops_.size() ? 0 : next_);
    }

    return loop;
}

// 获取当前线程池中所有线程分别对应的 loop
std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
    {
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    else
    {
        return loops_;
    }
}