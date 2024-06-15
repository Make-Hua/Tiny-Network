#pragma once

#include <vector>
#include <sys/epoll.h>

#include "Poller.h"

class Channel;

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