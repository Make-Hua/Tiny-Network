#pragma once

#include <vector>
#include <string>
#include <algorithm>


/// 缓冲区类模型
/// 
/// @code
/// +-------------------+------------------+------------------+
/// |     可预置字节     |      可读字节     |     可写字节     |
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode

// 网络层底层的缓冲器类型
class Buffer
{
public:
    // 说明缓冲区中共可以存 1032 字节
    static const size_t kCheapPrepend = 8;                          // 预置大小
    static const size_t kInitialSize = 1024;                        // 初始大小

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kInitialSize + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {}

    // 可读数据大小(长度)
    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    // 可写数据大小(长度)
    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    // 预置区域大小
    size_t prependableBytes() const 
    {
        return readerIndex_;
    }

    // 更新目前可读区域的大小
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            // 应用只读取了可读缓冲区的一部分，也就是 len, 还剩下 readerIndex_ += len
            readerIndex_ += len;
        }
        else
        {
            retrieveAll();
        }
    }

    // 重置读和写
    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // DEBUG使用，提取出string类型，但是不会置位
    std::string GetBufferAllAsString()
    {
        size_t len = readableBytes();
        std::string result(peek(), len);
        return result;
    }

    // 把 onMessage 函数上报的 Buffer 数据，转成 string 类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }   

    // 从 buffer 中取出长度为 len 的字节的数据
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    // buffer_.size() - writerIndex_ 可写区域已经不够写入，则进行扩容
    void ensureWriteableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            // 扩容函数
            makeSpace(len);
        }
    }
    
    // 把 [data, data + len] 内存上的数据，添加到 writable 缓冲区当中去
    void append(const char* data, size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }
    
    // 返回缓冲区中可读数据的起始地址
    const char* peek() const 
    {
        return begin() + readerIndex_;
    }

    char* beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char* beginWrite() const
    {
        return begin() + writerIndex_;
    }

    // 从 fd 上读取数据
    ssize_t  readFd(int fd, int* saveErrno);

    // 通过 fd 发送数据
    ssize_t  writeFd(int fd, int* saveErrno);

    // 在缓冲区中查找 CRLF (\r\n) 的位置
    const char* findCRLF() const {
        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
        return crlf == beginWrite() ? nullptr : crlf;
    }

    // 从缓冲区中提取数据，直到指定的终止位置
    void retrieveUntil(const char* end) {
        retrieve(end - peek());
    }

private:
    char* begin()
    {
        // 数组的起始地址
        return &*buffer_.begin();
    }

    const char* begin() const 
    {
        return &*buffer_.begin();
    }
    
    // 扩容
    void makeSpace(size_t len)
    {
        /**
         *   kCheapPrepend   |   reader   |   wridter   |
         *   kCheapPrepend   |           len            |
         */
        // if  : 可写区域 + 可读区域 - kCheapPrepend(预置区域) < len 
        // else: 整个 buffer 够用， 将已经读取后的 readable 移动到前面继续分配
        /**
         * 
         * kCheapPrepend   |   reader      |   wridter   |
         * kCheapPrepend   |[ 已读 ][ 未读 ]|   wridter   |
         *                     readerIndex_
         *     思路是 已读 + wridter 与 len 比较
        */
        if (writableBytes() + prependableBytes() - kCheapPrepend < len)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readalbe = readableBytes();
            std::copy(begin() + readerIndex_
                    , begin() + writerIndex_
                    , begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readalbe;
        }
    }

    std::vector<char> buffer_;              // buffer 缓冲区
    size_t readerIndex_;                    // 可读区域头下标
    size_t writerIndex_;                    // 可写区域头下标

    static const char kCRLF[];

};




