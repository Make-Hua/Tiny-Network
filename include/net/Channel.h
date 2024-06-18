#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;

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

    // 
    int fd() const { return fd_; }
    int events() const { return events_; }
    int set_revents(int revt) { revents_ = revt; }

    // 设置 fd 相应的事件状态 (记录 fd 感兴趣的事件)
    void enableReading() { events_ |= kReadEvent; update(); }           // 启用读事件
    void disableReading() { events_ &= ~kReadEvent; update(); }         // 禁用读事件
    void enableWriting() { events_ |= kWriteEvent; update(); }          // 启用写事件
    void disableWriting() { events_ &= ~kWriteEvent; update(); }        // 禁用写事件
    void disableAll() { events_ = kNoneEvent; update(); }               // 禁用所有事件

    // 返回 fd 当前事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }          // 检查当前事件是否没有任何事件（即无事件发生）
    bool isWriting() const { return events_ & kWriteEvent; }            // 检查当前事件是否包含写事件。
    bool isReading() const { return events_ & kReadEvent; }             // 检查当前事件是否包含读事件。

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
    static const int kNoneEvent;            // 读事件
    static const int kReadEvent;            // 写事件
    static const int kWriteEvent;           // 无任何事件

    
    EventLoop *loop_;                       // 事件循环
    const int fd_;                          // fd, Poller 监听的对象
    int events_;                            // 注册 fd 感兴趣的事件
    int revents_;                           // poller 返回具体发生的事件
    int index_;                             // 表示 kNew / kAdded / kDeleted

    std::weak_ptr<void> tie_;
    bool tied_;

    // 因为 channel 通道里面能够获知 fd 最终发生的具体的事件 revents
    // 所以它负责调用事件的具体回调操作
    ReadEventCallback readCallback_;        // 读事件回调
    EventCallback writeCallback_;           // 写事件回调
    EventCallback closeCallback_;           // 关闭事件回调
    EventCallback errorCallback_;           // 错误事件回调
};
