#include <stdlib.h>

#include "Poller.h"
#include "EPollPoller.h"


// EventLoop 事件循环可以通过该接口获取默认的 IO 复用的具体实现
Poller* Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL")) 
    {
        // Poll 实例
        return nullptr;
    }
    else
    {
        // Epoll 实例
        return new EPollPoller(loop);
    }
}