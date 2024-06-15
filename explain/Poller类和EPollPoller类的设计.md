# `Poller` 类和 `EPollPoller` 类的设计

### `Poller` 类

`Poller.h` 头文件中 `Poller` 类是方便于我们用同一个网络库且底层能使用不同 `IO` 复用 的一个向上分离出来的类，具体来说就是我们通过继承此虚类来实现具体的 `EPollPoller` 类、`SelectPoller` 类、`PollPoller` 类，通过枚举分别，在具体业务中通过对应 `EventLoop` 中使用指针的方式 `unique_ptr<Poller> poller_` 记录对应的 `Poller` 实例，然后通过 `newDefaultPoller()` 函数接口获取该 `Poller` 的具体实现类，通过运行时多态的形式实现。类的具体实现如下： 

```c++
// muduo 库中多路事件分发器的核心 IO 复用模块 
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);
    // 如果父类不写成虚函数，当子类调用析构函数时只会调用父类析构，从而导致子类的一些独有的变量得不到释放
    virtual ~Poller() = default;

    // 通过虚函数，让所有 IO 复用保留统一的接口（让具体的实现类 Epoll 类、 Poll 类、 Select 类 都用统一的接口）
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数 channel 是否在当前 Poller 中
    bool hashChannel(Channel *channel) const;

    // EventLoop 事件循环可以通过该接口获取默认的 IO 复用的具体实现
    static Poller* newDefaultPoller(EventLoop *loop);


protected:
    // map 存储的为             key -> sockfd   value -> 所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_;       // 定义 Poller 所属的事件循环 EventLoop
};


```

`Poller.cc` 源文件主要函数解读：

- `static Poller* newDefaultPoller(EventLoop *loop) ` 静态全局函数

上述函数主要是用来给 EventLoop 事件循环通过该接口获取默认的 IO 复用的具体实现。

> 注：具体的 PollPoller 类和 SelectPoller 类未实现

- `bool hashChannel(Channel *channel) const` 函数

上述函数主要是判断 `Channel` 是否在当前的 `Poller` 类中

```c++
Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{}

// 判断参数 channel 是否在当前 Poller 中
bool Poller::hashChannel(Channel *channel) const
{
    auto it = channels_.find(channel->fd());

    // 找到啦并且等于需要找的通道
    return it != channels_.end() && it->second == channel;
}


```



### `EPollPoller` 类

`EPollPoller.h` 头文件中 `EPollPoller` 类是继承于 `Poller` 类的，是 `Poller` 具体的实现类，其中会重写 `Poller` 类中的虚函数，从实现运行时多态，使得程序能够使用对应的 `IO` 复用。类中主要的方法还是对 Linux 的 epoll 的三个重要方法的一个封装。类的具体实现如下： 

```c++
/**
 * epoll 的使用 
 * epoll_create
 * epoll_ctl        add / del / mod
 * epoll_wait
 */
class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    // 重写抽象方法
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16;

    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

    // 更新 channel 通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;
    
    int epollfd_;               // epoll 对应的 listenfd
    EventList events_;          // 存储对应 epoll 上注册的 fd
};
```

`EPollPoller.cc` 源文件主要函数解读：

- `EPollPoller(EventLoop *loop)` 构造函数

构造函数用来初始化类的部分成员，同时通过 `epoll_create1()` 函数创建具体的 `epoll` 。

- `~EPollPoller()` 析构函数

析构函数需要关闭创建 `epoll` 需要用到的文件描述符 `epollfd_(lisenfd)` 。

- `Timestamp poll(int timeoutMs, ChannelList *activeChannels)` 函数

该函数主要是进行 `epoll` 的 `epoll_wait()` 函数， 该函数会以阻塞等待的方式，直到红黑树上的 `fd` 有事件发送或者超时，就会返回具体 `numEvents` ，之后通过调用 `fillActiveChannels()` 函数，将具体发送事件的 `fd` 事件对应的 `Channel` 存储到 `ChannelList` 列表中。 

- `void fillActiveChannels(int numEvents, ChannelList *activeChannels) const`  函数

该函数主要是将具体发送事件的 `fd` 事件对应的 `Channel` 存储到 `ChannelList` 列表中。 

- `void updateChannel(Channel *channel)` 和 `void removeChannel(Channel *channel)` 函数

该函数主要通过判断 `index_(kNew / kAdded / kDeleted)` 对应的值，从而输入不同的参数调用 updata() 函数。

> 注：说白了就是调用 epoll_ctl 执行 add / mod / del

- `void update(int operation, Channel *channel)` 函数

该函数通过传入具体的 `operation` 执行 `epoll_ctl()` 。

```c++
EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize)
{
    if (epollfd_ < 0) 
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size()) 
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else
    {
        if (saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (kNew == index || kDeleted == index)
    {
        if (kNew == index) 
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else 
    {
        int fd = channel->fd();
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_DEL, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);

    int index =  channel->index();
    if (kAdded == index)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}
    
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    // 下述操作，让 EventLoop 拿到了它的 Poller，然后给它返回了所有发生事件的 channel 列表了
    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

/*
typedef union epoll_data
{
  void *ptr;
  int fd;
  uint32_t u32;
  uint64_t u64;
} epoll_data_t;

struct epoll_event
{
  uint32_t events;
  epoll_data_t data;
} __EPOLL_PACKED;
*/

// 更新 channel 通道   epoll  add / mod / del
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event, sizeof event);

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (EPOLL_CTL_DEL == operation) 
        {
            LOG_ERROR("epoll_stl del error:%d \n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d \n", errno);
        }
    }
}

```

