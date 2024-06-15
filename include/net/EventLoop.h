#pragma once

#include <functional>
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>

#include "CurrentThread.h"
#include "noncopyable.h"
#include "Timestamp.h"

class Channel;
class Poller;


// 事件循环类  主要包含了 Channel  Poller（epoll抽象）
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();

    // 退出事件循环
    void quit();

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

    void handleRead();
    void doPendingFunctors();                               // 执行上层的回调函数

    using ChannelList = std::vector<Channel*>;

    // 基于 CAS 的原子操作
    std::atomic_bool looping_;
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