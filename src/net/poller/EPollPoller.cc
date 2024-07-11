#include <errno.h>
#include <unistd.h>
#include <strings.h>

#include "EPollPoller.h"
#include "Channel.h"
#include "asLogger.h"


const int kNew = -1;        // 表示从未添加
const int kAdded = 1;       // 表示已经添加
const int kDeleted = 2;     // 表示已经删除

/**
 *              EventLoop
 * 
 * 
 */

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
    // 应用 LOG_DEBUG 更合理
    LOG_INFO("func=%s => fd total count:%lu", __FUNCTION__, channels_.size());

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
    LOG_INFO("func=%s fd=%d events=%d index=%d", __FUNCTION__, channel->fd(), channel->events(), index);

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

    // LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);

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