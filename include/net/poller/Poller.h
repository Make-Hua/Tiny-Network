#pragma once

#include <vector>
#include <unordered_map>

#include "noncopyable.h"
#include "Timestamp.h"


class Channel;
class EventLoop;

// muduo 库中多路事件分发器的核心 IO 复用模块 
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);
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

