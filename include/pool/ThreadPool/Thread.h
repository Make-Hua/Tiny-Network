#pragma once

#include <functional>
#include <unistd.h>
#include <thread>
#include <memory>
#include <string>
#include <atomic>

#include "noncopyable.h"

// noncopyable 继承于此类的无法进行拷贝构造和拷贝赋值操作
class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();
	
    void start();
    void join();
	
    
    bool started() const {return started_; };			// 判断线程是否启动
    pid_t tid() const { return tid_; };					// 返回当前线程的线程 id
    const std::string& name() const { return name_; };	// 获取线程名字

    static int numCreated() { return numCreated_; };	// 已经创建多少线程

private:

    void setDefaultName();								// 设置默认名称

    bool started_;										// 线程是否启动相关 bool 值
    bool joined_;										// 
    std::shared_ptr<std::thread> thread_;				// 具体的线程
    pid_t tid_;											// 在线程创建时绑定
    ThreadFunc func_;									// 线程回调函数
    std::string name_;									// 线程名字

    static std::atomic_int numCreated_;					// 多个线程共享一个线程数量的值
};