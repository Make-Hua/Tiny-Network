# `EventLoop` 类的设计

### `EventLoop` 类

`EventLoop.h` 头文件中 `EventLoop` 类对应的是事件循环类，同时由于 `Channel` 类和 `EPollPoller` 类无法直接进行交互，所以同时也充当一个交互的中间层。在本类中，有一个比较经典的设计，就是 `wakeup` 的使用，待会会仔细讲解。

```c++
// 事件循环类  主要包含了 Channel  Poller（epoll抽象）
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();											// 开启事件循环
    void quit();											// 退出事件循环

    Timestamp poolReturnTime() const { return pollReturnTime_; }

    // 把 cb 放入队列中执行 cb
    void runInLoop(Functor cb);

    // 把 cb 放入队列中，唤醒 loop 所在的线程的，执行 cb
    void queueInLoop(Functor cb);

    // 唤醒 loop 所在的线程的
    void wakeup();

    // EventLoop 调用 Poller 的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);
    
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:

    void handleRead();										// 
    void doPendingFunctors();                               // 执行上层的回调函数

    using ChannelList = std::vector<Channel*>;
    
    std::atomic_bool looping_;								// 基于 CAS 的原子操作
    std::atomic_bool quit_;                                 // 标识退出 loop 循环

    const pid_t threadId_;                                  // 记录当前 loop 所在线程的 id

    Timestamp pollReturnTime_;                              // poller 返回发生事件的 channels 的时间点
    std::unique_ptr<Poller> poller_;                        // EventLoop 所管理的 Poller，而 poller 帮 EventLoop 监听所有发生事件

    int wakeupFd_;                                          // 作用：当 mainLoop 获取一个新用户的 channel, 通过轮询算法选择一个 subloop 来处理 channel
    std::unique_ptr<Channel> wakeupChannel_;                // wakeupFd_ 文件描述符对应的 Channel

    ChannelList activeChannels_;                            // 返回 Poller 监听到的有具体事件发生的 fd(Channel)

    std::atomic_bool callingPendingFunctors_;               // 标识当前 loop 是否需要执行的回调操作
    std::vector<Functor> pendingFunctors_;                  // 存储 loop 需要执行的所有的回调操作
    std::mutex mutex_;                                      // 互斥锁用来保证上 vec 容器的安全操作
};
```



`EventLoop.cc` 源文件提供给 `Channel` 类和 `EPollPoller` 类**交互的函数**解读：

- `void updateChannel(Channel *channel)` 

该函数提供给 `EPollPoller` 类调用 `updateChannel()` 。

- `void removeChannel(Channel *channel) `

该函数提供给 `EPollPoller` 类调用 `removeChannel()` 。

- `bool hasChannel(Channel *channel)` 

 该函数提供给 `EPollPoller` 类调用 `hasChannel()` 。



`EventLoop.cc` 源文件**主要函数**解读：

- `EventLoop()` 构造函数

构造函数主要是创建一个 `EPollPoller` 的实例 `poller`，同时创建 `wakeupFd` 以及对应的 `wakeupChannel_` 。

- `~EventLoop()` 析构函数

析构函数主要是把 `wakefd` 进行销毁处理。分别需要将 `wakefd` 取消监听以及 `close` 。

- `void loop()` 函数

该函数是 `EventLoop` 类的主要事件循环函数，主要是通过调用 `EPollPoller::poll()` 进行一个 `epoll_wait` 操作，同时将所有活跃的 `fd` 传给我们的 `ChannelList` ，然后在对活跃的 `fd` 执行一个具体的回调，最后还需要执行 `doPendingFunctors()` 去处理当前 `EventLoop` 事件循环所需要处理的回调。

> 注：此处需要处理两种回调，一种是有 `EPollPoller` 所监听到的活跃的 `fd` 对应的回调，最终会在 `Channel::handleEventWithGuard` 中执行；另一种是由上层 `TcpServer` 和 `TcpConnection` 所注册的回调，最终会在 `EventLoop::doPendingFunctors()` 中统一处理。

- `void quit()` 函数

该函数用来退出事件循环，由于有可能存在是别的 `loop` 事件循环中要退出当前事件循环，所以需要将当前事件循环进行 `wakeup` 操作。

- `void doPendingFunctors()` 函数

该函数又来处理所有的上层所注册的回调，上层对应回调会写入 `vector<Functor>` 中，然后挨个执行。在这里涉及到一个巧妙的优化，由于可能是在多线程环境中执行，当我们去挨个枚举 `vector` 时可能存上层继续往当前 `vector` 进行加入回调的操作，所有我们在循环枚举的时候需要加上一把锁，但是这样 **锁的粒度** 会太大，需要等待所有 `Func` 执行完毕才会解锁，所有为了降低我们锁的粒度，我们会将 `vector` 中的待执行的 Func 存放到临时的 `vec` 中，然后在执行，这样就大大降低了锁的粒度。

但是这样做，又会引出一个问题，在开始的想法中，我们是在进入该函数就会加锁，直到遍历完 `vector` 中的所有 `Func` 并执行完后才会解锁，这样的话其他线程中如果需要对 `vector` 进行操作就会阻塞，直到该函数执行完毕（锁释放）。但是我们现在是通过创建临时 `vec` 的操作减小锁的粒度，也就是说，当我们去执行临时 `vec` 中的 `Func` 时是没有加锁的，这就会导致此时在其他线程中会向我们的 `vector` 中加入回调函数，此时这些回调函数会在下一次 `loop` 事件循环中执行，但是在 `loop` 中的 `while` 循环我们是先执行 `epoll_wait` ，会阻塞等待具体 `fd` 事件的发生，则这些 Func 不会第一时间执行，而是有新的 `fd` 事件发生才会。在此，我们引出 `wakeup` 的概念。 



`EventLoop.cc` 源文件 **wakeup 相关函数** 解读：

- `void runInLoop(Functor cb)` 函数

该函数提供给上层 `TcpServer` 和 `TcpConnection` 进行加入执行回调函数的接口。

- `void queueInLoop(Functor cb)` 函数

该函数把不是当前线程对应的回调统一注册到 vector 中，最后在判断 `(!isInLoopThread() || callingPendingFunctors_)` 确定是否需要进行 `wakeup` 操作。

- `void wakeup()` 函数

该函数用来给已经注册好的 wakeupFd_ 写入一个数据，这样就能让对应的 loop 中的 `epoll_wait`  不在阻塞，能够往下继续执行，从而保证上层往 vector 添加的函数能够快速执行。

- `void handleRead()` 函数

该函数时 wakeupFd_ 发生读写事件的具体回调函数。

```c++
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
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
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
        // 监听两类 fd，一种是与客户端之间通信的 fd，另一种是 mainloop 和 subloop 之间通信的 fd
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

```

