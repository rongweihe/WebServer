# A C++ Lightweight Web Server

[![license](https://camo.githubusercontent.com/b0224997019dec4e51d692c722ea9bee2818c837/68747470733a2f2f696d672e736869656c64732e696f2f6769746875622f6c6963656e73652f6d6173686170652f6170697374617475732e737667)](https://opensource.org/licenses/MIT) [![Build Status](https://camo.githubusercontent.com/8c124d402beec8fe8fa404add68e7d8028ab6719/68747470733a2f2f7472617669732d63692e6f72672f4d617276696e4c652f5765625365727665722e7376673f6272616e63683d6d6173746572)](https://travis-ci.org/MarvinLe/WebServer)

## 简介

这是一个轻量级的 Web 服务器，目前支持 GET、HEAD 方法处理静态资源。并发模型选择: 线程池+Reactor+非阻塞方式运行。

测试页面: <demo>

------

| Part Ⅰ       | Part Ⅱ           |
| ------------ | ---------------- |
| [整体设计](https://github.com/rongweihe/WebServer/blob/master/%E6%95%B4%E4%BD%93%E8%AE%BE%E8%AE%A1.md) | [性能测试分析](https://github.com/rongweihe/WebServer/blob/master/%E6%B5%8B%E8%AF%95.md) |

------

## 开发部署环境

- 操作系统: Ubuntu 5.4.0-6 Ubuntu16.04.9
- 编译器: g++ version 5.4.0 20160609
- 版本控制: git
- 编辑器: Vim
- 压测工具：[WebBench](https://github.com/EZLippi/WebBench)

## Usage

```
cmake . && make 

./webserver [-p port] [-t thread_numbers]  [-r website_root_path] [-d daemon_run]
```

## 核心功能及技术

- 状态机解析 HTTP 请求，目前支持 HTTP GET、HEAD 方法
- 使用 priority 队列实现的最小堆结构管理定时器，使用标记删除
- 使用 epoll + 非阻塞IO + 边缘触发(ET) 实现高并发处理请求，使用 Reactor 编程模型
- epoll 使用 EPOLLONESHOT 保证一个 socket 连接在任意时刻都只被一个线程处理
- 使用多线程充分利用多核 CPU，并使用线程池避免线程频繁创建销毁的开销
- 为减少内存泄漏的可能，使用智能指针等 RAII 机制

## 开发计划

- 添加异步日志系统，记录服务器运行状态
- 自动化构建: cmake
- 集成开发工具: CLion

## 参考
- https://github.com/linyacool/WebServer
