LinuxWebserver
=============
Linux下C++轻量级Web服务器，用于实践网络编程

* 使用 **线程池 + 非阻塞socket + epoll(ET和LT均实现) + 事件处理(Reactor和模拟Proactor均实现)** 的并发模型
* 使用**状态机**解析HTTP请求报文，支持解析**GET和POST**请求
* 访问服务器数据库实现web端用户**注册、登录**功能，可以请求服务器**图片和视频文件**
* 实现**同步/异步日志系统**，记录服务器运行状态
* 经Webbench压力测试可以实现**上万的并发连接**数据交换

框架
==============
![框架示意图](root/frame.jpg)

快速运行
==============
* 服务器测试环境
    * Ubuntu 20.04.6 LTS
    * mysql  Ver 8.0.36
* 浏览器测试环境
    * Windows Linux均可
    * chrome
    * edge
* 测试前确认已安装mysql数据库和mysql C API
````mysql
#创建yourdb
create database yourdb;
#创建user_info表
use yourdb;
create table user_info(
    username char(50) NULL,
    passwd char(50) NULL
);
`````
* 修改main.cpp中数据库
````CPP
//数据库登录名,密码,库名
std::string user = "root";
std::string passwd = "root";
std::string databasename = "yourdb";
``````
* 构建 编译
````shell
mkdir build
cd build
cmake ../
make
`````
* 启动
````shell
./Webserver
`````
配置运行
============
````shell
./Webserver [-p port] [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model]
`````
* -p，自定义端口号
	* 默认9006
* -l，选择日志写入方式，默认同步写入
	* 0，同步写入
	* 1，异步写入
* -m，listenfd和connfd的模式组合，默认使用LT + LT
	* 0，表示使用LT + LT
	* 1，表示使用LT + ET
    * 2，表示使用ET + LT
    * 3，表示使用ET + ET
* -o，优雅关闭连接，默认不使用
	* 0，不使用
	* 1，使用
* -s，数据库连接数量
	* 默认为8
* -t，线程数量
	* 默认为8
* -c，关闭日志，默认打开
	* 0，打开日志
	* 1，关闭日志
* -a，选择反应堆模型，默认Proactor
	* 0，Proactor模型
	* 1，Reactor模型
 示例:
````shell
./Webserver -p 8888 -l 1 -m 3 -o 1 -s 10 -t 15 -c 0 -a 1
````
致谢
===============
* 《Linux高性能服务器编程》 游双著
*  我学习的项目:[Tinywebserver](https://github.com/qinguoyi/TinyWebServer)
