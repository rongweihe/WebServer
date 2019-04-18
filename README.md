# A C++ Lightweight Web Server

[![license](https://camo.githubusercontent.com/b0224997019dec4e51d692c722ea9bee2818c837/68747470733a2f2f696d672e736869656c64732e696f2f6769746875622f6c6963656e73652f6d6173686170652f6170697374617475732e737667)](https://opensource.org/licenses/MIT) [![Build Status](https://camo.githubusercontent.com/8c124d402beec8fe8fa404add68e7d8028ab6719/68747470733a2f2f7472617669732d63692e6f72672f4d617276696e4c652f5765625365727665722e7376673f6272616e63683d6d6173746572)](https://travis-ci.org/MarvinLe/WebServer)

## 简介

这是一个轻量级的 Web 服务器，目前支持 GET、HEAD 方法处理静态资源。并发模型选择: 单进程＋Reactor+非阻塞方式运行。

测试页面: <demo>

------

| Part Ⅰ       | Part Ⅱ           |
| ------------ | ---------------- |
| [整体设计]() | [性能测试分析](https://github.com/rongweihe/WebServer/blob/master/%E6%B5%8B%E8%AF%95.md) |

------

## 开发部署环境

- 操作系统: Ubuntu 16.04
- 编译器: g++ 5.4
- 版本控制: git
- 自动化构建: cmake
- 集成开发工具: CLion
- 编辑器: Vim
- 压测工具：[WebBench](https://github.com/EZLippi/WebBench)

## Usage

```
cmake . && make 

./webserver [-p port] [-t thread_numbers]  [-r website_root_path] [-d daemon_run]
```

## 核心功能及技术

- 状态机解析 HTTP 请求，目前支持 HTTP GET、HEAD 方法
- 添加定时器支持 HTTP 长连接，定时回调 handler 处理超时连接
- 使用 priority queue 实现的最小堆结构管理定时器，使用标记删除，以支持惰性删除，提高性能
- 使用 epoll + 非阻塞IO + 边缘触发(ET) 实现高并发处理请求，使用Reactor编程模型
- epoll 使用 EPOLLONESHOT 保证一个 socket 连接在任意时刻都只被一个线程处理
- 使用线程池提高并发度，并降低频繁创建线程的开销
- 同步互斥的介绍
- 使用RAII手法封装互斥器 (pthrea_mutex_t)、 条件变量(pthread_cond_t)等线程同步互斥机制，使用RAII管理文件描述符等资源
- 使用 shared_ptr、weak_ptr 管理指针，防止内存泄漏

## 开发计划

- 添加异步日志系统，记录服务器运行状态
- 增加 json 配置文件，支持类似 nginx 的多网站配置
- 提供 CGI 支持
- 类似 nginx 的反向代理和负载均衡
- 必要时增加可复用内存池
