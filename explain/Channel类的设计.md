# `Channel` 类的设计

## `Channel` 类前言

首先 `Channel` 类最重要就是其实就是文件描述符  `const int fd_;` ，其实说白了 `Channel` 类是对每一个 `fd` 的一个封装。当然除了封装，在类中还有 `fd` 对应事件的一些回调，当 `fd` 发生相应的事件时，就会调用其对应回调。`Channel` 类与代码中与 `EventLoop` 类和 `EPollPoller` 类在整个项目中是强相关的，其分别对应的就是 `Reactor` 模型中的反应堆以及事件分发器。

> 注：请先阅读完 `Reactor` 模型讲解后在学习具体代码

## `Channel` 类

`Channel.h` 头文件中 `Channel` 类就是对文件描述符的一个封装，同时也封装了对应事件的回调方法，供上层调用。

```c++
/**
 * 需要理清  EventLoop  Channel  Poller 之间的关系
 * channel 可以理解为通道，封装了 sockfd 和其感兴趣的 event
 * 如 EPOLLIN、EPOLLOUT 事件
 * 还绑定了 poller 返回的具体事件
 */
class Channel : noncopyable
{
public:
    // 具体绑定的回调
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd 得到 poller 的通知后，进行处理事件的函数
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 防止当 channel 被手动 remove 掉，channel 还在执行回调
    void tie(const std::shared_ptr<void>&);

    // 获取和设置
    int fd() const { return fd_; }
    int events() const { return events_; }
    int set_revents(int revt) { revents_ = revt; }

    // 设置 fd 相应的事件状态 (记录 fd 感兴趣的事件)
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    // 返回 fd 当前事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    // 
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop* ownerLoop() { return loop_; }
    void remove();

private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    // 由于为多线程服务器，则 可以有多个 poller(epoll), 则不同的 epoll 上可以监听不同事件的 fd
    static const int kNoneEvent;        // 读事件
    static const int kReadEvent;        // 写事件
    static const int kWriteEvent;       // 无任何事件

    
    EventLoop *loop_;                   // 事件循环
    const int fd_;                      // fd, Poller 监听的对象
    int events_;                        // 注册 fd 感兴趣的事件
    int revents_;                       // poller 返回具体发生的事件
    int index_;                         // 

    std::weak_ptr<void> tie_;
    bool tied_;

    // 因为 channel 通道里面能够获知 fd 最终发生的具体的事件 revents
    // 所以它负责调用事件的具体回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

```

`Channel.cc` 源文件主要函数解读：

- `Channel(EventLoop *loop, int fd) ` 构造函数

构造函数主要是对类内容进行初始化，其中 `Channel` 类的一个对象会有唯一的一个 `fd_` ，同时每一个 Channel 类的实例对象都会有一个 `EventLoop *` 执行所对应的 `EventLoop(Reactor 反应堆)` 。一个 `EventLoop` 对应多个 `Channel` 实例，而一个 `Channel` 实例只会对应一个 `EventLoop`。

- `void update()` 函数

该函数会去更新 `Channel` 对象中 `fd_` 所关注的事件。由于 `Channel` 不会直接更 `EPollPoller` 类直接交互，而是间接交互，所以当我们需要更新 `epoll` 红黑树上的事件时，我们执行的流程为：首先 `Channel` 会通过对应的 `set` 函数修改对应 `fd_` 所关注的事件，同时通过 `update()` 函数进行更新，然后在 `EventLoop` 中会去调用 `EPollPoller` 中的 `updateChannel()` 函数，最终进行更新。

- `void remove()` 函数

该函数会把 `Channel` 对象中 `fd_` 从 `epoll` 红黑树上删除。调用过程同上。

- `void handleEventWithGuard(Timestamp receiveTime)` 函数

上述函数是在 `EventLoop` 事件循环中，当 `epoll` 红黑树上所有的 `fd` 发送事件时，内核会将发送事件的 `fd` 传入双向链表中，最终传给上层。被 `EventLoop` 监听到后，然后由对应的 `EventLoop` 执行 `handleEvent()` 函数去调用下层的 `handleEventWithGuard()` ，最后由 `Channel` 执行对应的回调函数。

```c++
const int Channel::kNoneEvent = 0;                          // 读事件
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;         // 写事件
const int Channel::kWriteEvent = EPOLLOUT;                  // 无任何事件


// EventLoop ChannelList Poll 
Channel::Channel(EventLoop *loop, int fd) 
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false)
{}

Channel::~Channel() {}

// channel 的 tie 方法什么时候调用过 (在该处不讲)
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

// 当改变 channel 所表示 fd 的 events 事件后，update 负责在 poller 里面更改 fd 相应的事件 epoll_ctl
// EventLoop => ChannelList   Poller
void Channel::update()
{
    // 通过 channel 所属的 EventLoop,调用 poller 的相应方法，注册 fd 的events事件
    loop_->updateChannel(this);
}

// 在 channel 所属的 EventLoop 中，将当前 channel 删除
void Channel::remove()
{
    loop_->removeChannel(this);
}
 

// fd 得到 poller 的通知后，进行处理事件的函数
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_) 
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else 
    {
        handleEventWithGuard(receiveTime);
    }
}

// 根据 poller 监听所通知的 channel 发生的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    // 关闭事件
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) 
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }

    // 错误事件
    if (revents_ & EPOLLERR) 
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }

    // 读事件
    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    // 写事件
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}
```


