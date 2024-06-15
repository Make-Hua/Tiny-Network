

#include "Poller.h"
#include "Channel.h"


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

