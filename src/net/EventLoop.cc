#include <sys/eventfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory>
#include <errno.h>

#include "EventLoop.h"
#include "asLogger.h"
#include "Poller.h"
#include "Channel.h"


// 防止一个线程创建多个 EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的 Poller IO复用接口的超时时间(10s)
const int kPollTimeMs = 10000;

// 
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , timerQueue_(new TimerQueue(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in therad %d \n", this, threadId_)
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置 wakeupfd 事件 
    wakeupChannel_->setReadCallback(
        std::bind(&EventLoop::handleRead, this)
    );
    
    // 每一个 eventloop 都将监听 wakeupchannel 的 EPOLLIN 读事件了
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    // channel移除所有感兴趣事件 将channel从EventLoop中删除
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();

    // 关闭 wakeupFd_   指向EventLoop指针为空
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    // 记录日志
    LOG_INFO("EventLoop %p start looping \n", this);

    while (!quit_) 
    {
        activeChannels_.clear();
        // 监听两类 fd，一种是与客户端之间通信的 fd，另一种是 mainloop 和 subloop 之间通信的 fd （epoll_wait）
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            // Poller 监听哪些 channel 发生了事件，然后上报给 EventLoop，通知 channel 处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }

        // 执行当前 EventLoop 事件循环需要处理的回调操作
        doPendingFunctors();
    }

    // 记录日志
    LOG_INFO("EventLoop %p stop looping \n", this);
    looping_ = false;
}

//                          mainloop

//              subloop     subloop       subloop
/**
 *  退出事件循环
 *  1. loop 在自己的线程中调用 quit
 *  2. 如果是在其他线程中(subloop的work线程中调用mainloop的quit)
 * */
void EventLoop::quit()
{
    quit_ = true;

    // 如果需要 quit 的 EventLoop 并不是自己对应的线程，则唤醒要关闭的 EventLoop
    if (!isInLoopThread())
    {
        wakeup();
    }
}

// 把 cb 放入队列中执行 cb
void EventLoop::runInLoop(Functor cb)
{
    // 在当前的 loop 线程中，执行 cb
    if (isInLoopThread())
    {
        cb();
    }
    else        // 在非 loop 线程中执行 cb, 则唤醒loop所在线程中执行 cb
    {
        queueInLoop(cb);
    }
}

// 把 cb 放入队列中，唤醒 loop 所在的线程的，执行 cb
void EventLoop::queueInLoop(Functor cb)
{
    // 存在多个不同的 loop 去执行某个 loop 线程的 cb
    // 所以需要考虑 mutex
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的，需要执行上面的回调操作
    // || 上 call... 的意思是，当前 loop 正在执行回调，但是loop又有新的回调，所以需要wait一下
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        // 唤醒对应的 loop 线程
        wakeup();
    }
}

/*         wakeup 的作用：①能够保证所有回调及时处理 -》比如因为锁的问题导致部分函数没执行  -》比如当事件不为当前 loop 则可以及时唤醒对应 loop 并执行                     */

// 唤醒 loop 所在的线程的    向 wakeup fd 写一个数据
// wakeupChannel 就发生读事件，当前 loop 线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %zd bytes instead of 8 \n", n);
    }
}

// EventLoop 调用 Poller 的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hashChannel(channel);
}

// 执行回调
void EventLoop::doPendingFunctors()
{
    /**
     *  为什么在此需要创建一个局部的 vec 进行 swap 操作呢？
     *  因为某个线程需要执行回调操作的时候，可能其他线程此时会向 pendingFunctors_ 里添加回调操作，
     *  而为了保证容器的安全性我们需要进行加锁，如果不用零时 vec 的话，其他 loop线程 会阻塞于此，
     *  等待回调操作执行完毕，降低了执行速度，所以我们需要临时 vec
    */
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    // 保证锁的颗粒度尽可能的小
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor : functors) 
    {
        // 执行当前 loop 需要执行的回调操作
        functor();
    }

    callingPendingFunctors_ = false;
}



void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %zd bytes instead of 8", n);
    }
}



/**
 * 定时任务相关函数
 */

// 在指定的 timestamp 时间点执行回调函数 cb
void EventLoop::runAt(Timestamp timestamp, Functor&& cb) {
    timerQueue_->addTimer(std::move(cb), timestamp, 0.0);
}

// 在当前时间 waitTime 秒之后执行回调函数 cb
void EventLoop::runAfter(double waitTime, Functor&& cb) {
    Timestamp time(addTime(Timestamp::now(), waitTime)); 
    runAt(time, std::move(cb));
}

// 以 interval 秒为周期，定期执行回调函数 cb
void EventLoop::runEvery(double interval, Functor&& cb) {
    Timestamp timestamp(addTime(Timestamp::now(), interval)); 
    timerQueue_->addTimer(std::move(cb), timestamp, interval);
}