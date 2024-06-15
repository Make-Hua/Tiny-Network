#include "CurrentThread.h"

namespace CurrentThread
{
    __thread int t_cachedTid = 0;

    void cacheTid()
    {
        if (0 == t_cachedTid)
        {
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}
/**
 * 作用和功能：
定义线程本地存储变量：

__thread int t_cachedTid = 0; 定义了 t_cachedTid，初始值为 0。这意味着在每个线程中，t_cachedTid 初始为 0。
实现函数 cacheTid：

void cacheTid() 实现了缓存当前线程 ID 的功能。如果 t_cachedTid 为 0（表示未缓存），则通过 syscall(SYS_gettid) 系统调用获取当前线程的 ID，并将其赋值给 t_cachedTid。
总结
优化性能： 使用线程本地存储缓存线程 ID，减少频繁调用 syscall(SYS_gettid) 的开销，从而提高性能。
线程安全： __thread 确保每个线程有自己独立的 t_cachedTid 变量，避免了线程之间的竞争和冲突。
内联函数和分支预测优化： inline int tid() 和 __builtin_expect 进一步优化了性能，通过分支预测提示和内联函数减少函数调用开销。
该代码适用于需要频繁获取当前线程 ID 的场景，通过缓存和优化技术，提高了程序的运行效率。
 * 
 * 
*/