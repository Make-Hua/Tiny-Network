#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

#include "Buffer.h"


/**
 *  从 fd 上读取数据    Poller 工作模式未 LT 模式
 *  Buffer 缓冲区有大小的！ 但是从 fd 上读取数据的时候，却不知道 tcp 数据最终大小
 * 
*/
ssize_t  Buffer::readFd(int fd, int* saveErrno)
{
    // 栈上内存空间(64K)
    char extrabuf[65536] = {0};

    struct iovec vec[2];

    // Buffer 底层缓冲区剩余可写大小
    const size_t writable = writableBytes();

    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    // readv 高效

    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable)  // 已经够了
    {
        writerIndex_ += n;
    }
    else   // 不够，往 extrabuf 里面写入了部分数据
    {
        // extrabuf 里面也写入了数据
        writerIndex_ = buffer_.size();

        // writerIndex_ 开始写 n - writable 大小的数据
        append(extrabuf, n - writable);
    }

    return n;
}

// 通过 fd 发送数据
ssize_t  Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}