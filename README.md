# 基于非阻塞IO多路复用的高性能网络编程库

## 项目介绍

本项目是参照muduo仿写的基于主从Reactor模型的多线程网络库。使用c++11的新特性编写，去除了muduo对boost的依赖。目前项目已经实现了Channel模块、Poller模块、EventLoop模块、TcpServer模块、Buffer模块、日志模块。

## 项目结构

```shell
# 代码文件主要结构
TinyNetwork/
├── include/
│   ├── base/
│   ├── http/
│   ├── logger/
│   ├── net/
│   │   │── poller/
│   │   └── timer/
│   └── pool/
│       └── ThreadPool/
├── src/
│   ├── base/
│   ├── http/
│   ├── logger/
│   ├── net/
│   │   │── poller/
│   │   └── timer/
│   └── pool/
│       └── thread/
├── lib/
└── CMakeLists.txt
```

## 项目特点

- 网络编程库底层采用 Epoll + LT 模式的 I/O 复用模型，并且结合非阻塞 I/O 实现从 Reactor 模型，实现了高并发和高吞吐量
- 网络库采用了 one loop per therad 线程模型，并且向上封装线程池避免了线程的创建和销毁的性能开销，保证服务器的性能
- 使用 C++11 的新特性编写，对比 muduo 网络库，该网络库去除了对于 Boost 库的依赖，实现了更加轻量化的设计
- 采用 eventfd 作为事件通知描述符，通过 wakeup 机制巧妙的高效派发事件到其他线程执行异步任务
- 设计 Buffer 类，通过调用 readv API 减少系统调用次数，利用 buffer + extrabuf 设计提高访问速度，减少内存碎片
- 基于红黑树实现定时器的管理结构，内部使用 Linux 的 timerfd 通知到期任务，从而进行高效管理定时任务



## 开发环境

- 操作系统：`Ubuntu 18.04.2`
- 编译器：`g++ 7.5.0`
- 编译器：`vscode`
- 版本控制：`git`
- 项目构建：`cmake 3.10.2 `

## 并发模型

`Reactor` 模型：

![Buffer](https://github.com/Make-Hua/Tiny-Network/blob/master/image/Reactor.png)

`muduo`-`Reactor` 模型：

![Buffer](https://github.com/Make-Hua/Tiny-Network/blob/master/image/muduo-Reactor%E6%A8%A1%E5%9E%8B.png)

## 构建项目

依次运行脚本，其会自动编译项目

```shell
# 编译项目，生成 *.so 文件
sudo ./autobuild.sh

# 统一将 include 头文件加入系统编译时自动搜索路径 /usr/include
# 统一将 *.so 动态库加入系统编译时自动搜索路径 /usr/lib
sudo ./copy.sh
```

## `Tcp` 连接运行案例

依次执行以下指令：

```bash
# 进入目录
cd /example/tcp/

# 执行 makefile
make

# 运行 testserver
./testserver


# 新建终端进行连接 (默认 ip = 127.0.0.1   prot = 8080)
nc 127.0.0.1 8080
```

执行结果如下图：

![Tcp连接运行案例](https://github.com/Make-Hua/Tiny-Network/blob/master/image/Tcp%E6%89%A7%E8%A1%8C%E5%9B%BE.png)


## `Http` 服务器运行案例

依次执行以下指令：

```bash
# 进入目录
cd /example/http/

# 执行 makefile
make

# 运行 testserver
./testhttpserver
```

进入 `linux` 自带浏览器，输入对应 `127.0.0.1:9789` 执行结果如下图：

![Http服务器运行案例](https://github.com/Make-Hua/Tiny-Network/blob/master/image/http%E6%89%A7%E8%A1%8C%E5%9B%BE.png)





## 项目优化

#### [异步日志](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/%E5%BC%82%E6%AD%A5%E6%97%A5%E5%BF%97%E8%AE%BE%E8%AE%A1%E8%AE%B2%E8%A7%A3.md)

#### [定时器](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/%E5%AE%9A%E6%97%B6%E5%99%A8%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

`MySQL` 连接池

`Memory` 内存池

`http` 服务器

## 项目讲解

模块一：

[`Channel` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/Channel%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

[`Poller` 和 `EPollPoller` 类的设计](https://github.com/Make-Hua/TinyNetwork/blob/master/explain/Poller_%E7%B1%BB%E5%92%8C_EPollPoller_%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

[`EventLoop` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/EventLoop%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

模块二：

[线程池的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/%E7%BA%BF%E7%A8%8B%E6%B1%A0%E8%AE%BE%E8%AE%A1%E8%AE%B2%E8%A7%A3.md)

模块三：

[`Acceptor` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/Acceptor%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

[`InetAddress` 和 `Sockt` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/InetAddress%E7%B1%BB%E5%92%8CSockt%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

模块四：

[`Buffer` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/Buffer%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

[`TcpConnection` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/TcpConnection%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

[`TcpServer` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/TcpServer%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)