# `Buffer` 类的设计

### `Buffer` 类前言

首先我会先说明为何要引进 `Buffer` 类，正如下述示例得知，在没有缓冲区的时候，服务器回送客户端消息的代码如下：

```c++
char buf[READ_BUFFER];
while(true){
	bzero(&buf, sizeof(buf));
	ssize_t bytes_read = read(sockfd, buf, sizeof(buf));
if(bytes_read > 0){
        // ...
	} else if(bytes_read == -1 && errno == EINTR){
		// ...
	} else if(bytes_read == -1 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))){
        // ...
    } else if(bytes_read == 0){
        // ...
    }
}
```

这是非阻塞式 `socket` IO的读取，可以看到使用的读缓冲区大小为1024，每次从TCP缓冲区读取1024大小的数据到读缓冲区，然后发送给客户端。这是最底层C语言的编码，在逻辑上有很多不合适的地方。比如我们不知道客户端信息的真正大小是多少，只能以1024的读缓冲区去读TCP缓冲区（就算TCP缓冲区的数据没有1024，也会把后面的用空值补满）；也不能一次性读取所有客户端数据，再统一发给客户端。

> 关于TCP缓冲区、`socket` IO读取的细节，在《UNIX网络编程》卷一中有详细说明，想要精通网络编程几乎是必看的

虽然以上提到的缺点以 `C` 语言编程的方式都可以解决，但我们仍然希望以一种更加优美的方式读写 `socket` 上的数据，和其他模块一样，脱离底层，让我们使用的时候不用在意太多底层细节，所以封装一个缓冲区是很有必要的。



### `Buffer` 类

`Buffer.h` 头文件中 `Buffer` 类就是单独设计出一个缓冲区出来让我们在网络层面收发信息时能够更加的优雅，能以一个更好的方案去处理。

```c++
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

    // 返回缓冲区中可读数据的起始地址
    const char* peek() const 
    {
        return begin() + readerIndex_;
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

};
```

`Buffer.cc` 源文件主要函数解读：

- `size_t readableBytes() const` 函数、`size_t writableBytes() const` 函数、`size_t prependableBytes() const` 函数

分别表示返回可读数据的长度、返回可写数据的长度、返回预置区域的大小。

- `char* begin()` 函数、`const char* begin() const`  函数

上述两个函数均是返回缓冲区的起始地址。

- `const char* peek() const` 函数、`char* beginWrite()` 函数、`const char* beginWrite() const` 函数

分别表示返回可读数据区域的起始地址，可写数据区域的起始地址。

- `string retrieveAllAsString()` 函数

该函数用来把 `onMessage` 函数上报的 `Buffer` 数据，转成 `string` 类型的数据返回。具体调用了 `retrieveAsString` 函数。

- `string retrieveAsString(size_t len)` 函数

该函数用来从 `buffer` 中取出长度为 `len` 个字节的数据，并且调用 `retrieve` 函数更新指向缓冲区的两个下标。

- `void retrieve(size_t len)` 函数

该函数用来更新指向缓冲区的两个下标。

- `void append(const char* data, size_t len)` 函数

该函数主要是将数据 `data` 添加到 `buffer` 中的 `writable` 区域中，可能会出现 `buffe`!r 区域不够的情况，这个时候会调用 `ensureWriteableBytes` 进行扩容。

- `void ensureWriteableBytes(size_t len)` 函数、`void makeSpace(size_t len)` 函数

`ensureWriteableBytes` 函数通过调用 `makeSpace` 函数进行扩容。

![image-20240617233535352](C:\Users\马克.华\AppData\Roaming\Typora\typora-user-images\image-20240617233535352.png)



- `ssize_t  readFd(int fd, int* saveErrno)` 函数

该函数通过调用 `readv` 函数进行从对应 `fd` 中读出数据，并且将数据写入缓冲区。在此出使用了 `iovec` 和  `extrabuf` 的组合，能够实现了高效的数据读取，既减少了系统调用次数，又通过栈上缓冲区的临时存储减少了动态内存分配和拷贝操作，从而提高了整体性能和代码的简洁性。该设计的亮点：

> **减少系统调用次数**
>
> `readv` 系统调用允许一次读取多个缓冲区的数据。相比于多次调用 `read`，一次调用 `readv` 可以显著减少系统调用的次数，从而降低系统调用带来的开销和延迟。这对于提高程序的性能特别重要。
>
> **减少内存分配和拷贝**
>
> 使用一个预分配的栈上缓冲区 `extrabuf` 可以减少动态内存分配和释放的次数。栈上缓冲区的分配和释放非常高效，并且其生命周期受限于函数调用周期，不会导致内存碎片问题。
>
> **提高局部性和缓存命中率**
>
> 栈上的缓冲区通常具有更好的局部性，这意味着它们更可能被缓存，访问速度更快。相比于动态分配的堆内存，栈上内存的访问速度更快，能够提高整体性能。

- `ssize_t  writeFd(int fd, int* saveErrno)` 函数

该函数通过调用 `write` 函数将缓冲区中的数据发生出去。

```c++
/**
 *  从 fd 上读取数据    Poller 工作模式未 LT 模式
 *  Buffer 缓冲区有大小的！ 但是从 fd 上读取数据的时候，却不知道 tcp 数据最终大小
*/
ssize_t  Buffer::readFd(int fd, int* saveErrno)
{
    // 单独开辟一个栈上内存空间(64K)
    char extrabuf[65536] = {0};

//    struct iovec {
//        void  *iov_base;    /* Starting address */
//        size_t iov_len;     /* Number of bytes to transfer */
//    };

    // 使用 iovec 
    struct iovec vec[2];
    const size_t writable = writableBytes();

    // 第一个缓冲区 vec[0] ， 对应的就是 buffer 中 可写缓冲区
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    // 第二个缓冲区 vec[1] , 对应的就是上面开辟的栈空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    // ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
    //           所要读取的 fd    包含若干块的 iovec   指定读 iovcnt 块 iovec

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
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
```

