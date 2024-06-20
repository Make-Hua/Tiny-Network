
#include <sys/timerfd.h>
#include <unistd.h>
#include <string.h>

#include "EventLoop.h"
#include "Channel.h"
#include "asLogger.h"
#include "Timer.h"
#include "TimerQueue.h"

int createTimerfd()
{
    /**
     * CLOCK_MONOTONIC：绝对时间
     * TFD_NONBLOCK：非阻塞
     */
    // linux内置文件描述符类型： 定时器文件描述符
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

    if (timerfd < 0)
    {
        LOG_ERROR("Failed in timerfd_create");
    }
    return timerfd;
}

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop_, timerfd_),
      timers_()
{
    timerfdChannel_.setReadCallback(
        std::bind(&TimerQueue::handleRead, this)
    );
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{   
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
    // 删除所有定时器
    for (const Entry& timer : timers_)
    {
        delete timer.second;
    }
}

void TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval)
{
    Timer* timer = new Timer(std::move(cb), when, interval);

    // 在事件循环中加入一个定时器
    loop_->runInLoop(
        std::bind(&TimerQueue::addTimerInLoop, this, timer)
    );
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
    // 是否取代了最早的定时触发时间
    bool eraliestChanged = insert(timer);

    // 我们需要重新设置 timerfd_ 触发时间
    if (eraliestChanged)
    {
        resetTimerfd(timerfd_, timer->expiration());
    }
}

// 重置timerfd
void TimerQueue::resetTimerfd(int timerfd_, Timestamp expiration)
{

    // struct itimerspec
    // {
    //     struct timespec it_interval;
    //     struct timespec it_value;

    //     struct timespec
    //     {
    //         __time_t tv_sec;		/* Seconds.  */
    //         __syscall_slong_t tv_nsec;	/* Nanoseconds.  */
    //     };
    // };

    struct itimerspec newValue;
    struct itimerspec oldValue;
    memset(&newValue, '\0', sizeof(newValue));
    memset(&oldValue, '\0', sizeof(oldValue));

    // 超时时间 - 现在时间
    int64_t microSecondDif = expiration.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    if (microSecondDif < 100)
    {
        microSecondDif = 100;
    }

    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microSecondDif / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>((microSecondDif % Timestamp::kMicroSecondsPerSecond) * 1000);
    newValue.it_value = ts;

    // 此函数会唤醒事件循环
    if (::timerfd_settime(timerfd_, 0, &newValue, &oldValue))
    {
        LOG_ERROR("timerfd_settime faield()");
    }
}

// 执行 read 读取数据，不然将一直阻塞于此
void ReadTimerFd(int timerfd) 
{
    uint64_t read_byte;
    ssize_t readn = ::read(timerfd, &read_byte, sizeof(read_byte));
    
    if (readn != sizeof(read_byte)) {
        LOG_ERROR("TimerQueue::ReadTimerFd read_size < 0");
    }
}

// 根据当前的时间戳，确定哪些定时任务到期，
// 然后返回并删除的定时器节点 （std::vector<Entry> expired）
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    std::vector<Entry> expired;

    // 创建一个哨兵（sentry）节点，用于查找所有已到期的定时器
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));

    // lower_bound 根据具体时间戳，返回第一个大于或等于哨兵节点的定时器迭代器
    TimerList::iterator end = timers_.lower_bound(sentry);

    // 将所有已到期的定时器节点复制到 expired 向量中并从定时器队列中删除已到期的定时器节点
    std::copy(timers_.begin(), end, back_inserter(expired));
    timers_.erase(timers_.begin(), end);
    
    return expired;
}

void TimerQueue::handleRead()
{
    Timestamp now = Timestamp::now();
    ReadTimerFd(timerfd_);

    std::vector<Entry> expired = getExpired(now);

    // 遍历到期的定时器，调用回调函数
    callingExpiredTimers_ = true;
    for (const Entry& it : expired)
    {
        it.second->run();
    }
    callingExpiredTimers_ = false;
    
    // 重新设置这些定时器
    reset(expired, now);

}

// 重新设置定时任务，重复任务则继续加入定时红黑树，一次任务直接删除
void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
    Timestamp nextExpire;
    for (const Entry& it : expired)
    {
        // 重复任务则继续执行
        if (it.second->repeat())
        {
            auto timer = it.second;
            timer->restart(Timestamp::now());
            insert(timer);
        }
        else
        {
            delete it.second;
        }

        // 如果重新插入了定时器，需要继续重置timerfd
        if (!timers_.empty())
        {
            resetTimerfd(timerfd_, (timers_.begin()->second)->expiration());
        }
    }
}

bool TimerQueue::insert(Timer* timer)
{
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first)
    {
        // 说明最早的定时器已经被替换了
        earliestChanged = true;
    }

    // 定时器管理红黑树插入此新定时器
    timers_.insert(Entry(when, timer));

    return earliestChanged;
}