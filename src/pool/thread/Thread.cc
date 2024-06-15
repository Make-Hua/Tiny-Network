#include <semaphore.h>

#include "Thread.h"
#include "CurrentThread.h"

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
    {
        // 分离线程的方法
        thread_->detach();
    }
}

// 一个 Thread 对象记录了一个线程的详细信息
void Thread::start()
{
    sem_t sem;
    sem_init(&sem, false, 0);

    started_ = true;
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]{
        // 获取线程 tid 值
        tid_ = CurrentThread::tid();
        
        sem_post(&sem);

        // 开启一个新线程专门执行该函数
        func_();
    }));

    // 必须等待获取上面新创建的线程
    sem_wait(&sem);
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}


void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread %d", num);
        name_ = buf;
    }
}