# 基于非阻塞IO多路复用的高性能网络编程库

## 项目介绍

本项目是参照muduo仿写的基于主从Reactor模型的多线程网络库。使用c++11的新特性编写，去除了muduo对boost的依赖。目前项目已经实现了Channel模块、Poller模块、EventLoop模块、TcpServer模块、Buffer模块、日志模块。

## 项目结构

```shell
# 代码文件主要结构
TinyNetwork/
├── include/
│   ├── base/
│   ├── logger/
│   ├── net/
│   │   └── poller/
│   └── pool/
│       └── ThreadPool/
├── src/
│   ├── base/
│   ├── logger/
│   ├── net/
│   │   └── poller/
│   └── pool/
│       └── thread/
├── lib/
└── CMakeLists.txt
```

## 项目特点

- 网络编程库底层采用 Epoll + LT 模式的 I/O 复用模型，并且结合非阻塞 I/O 实现从 Reactor 模型，实现了高并发和高吞吐量
- 网络库采用了 one loop per therad 线程模型，并且向上封装线程池避免了线程的创建和销毁的性能开销，保证服务器的性能
- 采用 eventfd 作为事件通知描述符，方便高效派发事件到其他线程执行异步任务
- 使用 C++11 的新特性编写，对比 muduo 网络库，该网络库去除了对于 Boost 库的依赖，实现了更加轻量化的设计

## 开发环境

- 操作系统：`Ubuntu 18.04.2`
- 编译器：`g++ 7.5.0`
- 编译器：`vscode`
- 版本控制：`git`
- 项目构建：`cmake 3.10.2 `

## 并发模型

`Reactor` 模型


## 构建项目

依次运行脚本，其会自动编译项目

```shell
# 编译项目，生成 *.so 文件
sudo ./autobuild.sh

# 统一将 include 头文件加入系统编译时自动搜索路径 /usr/include
# 统一将 *.so 动态库加入系统编译时自动搜索路径 /usr/lib
sudo ./copy.sh
```

## 运行案例



## 项目优化

#### [异步日志](https://github.com/Make-Hua/TinyNetwork/blob/master/explain/%E5%BC%82%E6%AD%A5%E6%97%A5%E5%BF%97%E8%AE%BE%E8%AE%A1%E8%AE%B2%E8%A7%A3.md)

定时器

`MySQL` 连接池

`Memory` 内存池

`http` 服务器

## 项目讲解

模块一：

[`Channel` 类的设计](https://github.com/Make-Hua/TinyNetwork/blob/master/explain/Channel%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

[`Poller` 和 `EPollPoller` 类的设计](https://github.com/Make-Hua/TinyNetwork/blob/master/explain/Poller_%E7%B1%BB%E5%92%8C_EPollPoller_%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

[`EventLoop` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/EventLoop%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

模块二：

[线程池的设计](https://github.com/Make-Hua/TinyNetwork/blob/master/explain/%E7%BA%BF%E7%A8%8B%E6%B1%A0%E8%AE%BE%E8%AE%A1%E8%AE%B2%E8%A7%A3.md)

模块三：

[`Acceptor` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/Acceptor%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

[`InetAddress` 和 `Sockt` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/InetAddress%E7%B1%BB%E5%92%8CSockt%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

模块四：

[`Buffer` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/Buffer%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

[`TcpConnection` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/TcpConnection%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)

[`TcpServer` 类的设计](https://github.com/Make-Hua/Tiny-Network/blob/master/explain/TcpServer%E7%B1%BB%E7%9A%84%E8%AE%BE%E8%AE%A1.md)