//
// Created by elysia on 3/11/24.
//
#ifndef CONFIG_H
#define CONFIG_H
#include "webserver.h"
class Config{
public:
    Config();
    ~Config(){};
public:
    void parse_args(int argc,char*argv[]);
public:
    int port; //端口号

    int LOGWrite; //日志写入方式

    int TRIGMode; //组合触发模式

    int LISTENTrigmode;  //listenfd 触发模式

    int CONNTrigmode; //connfd 触发模式

    int OPT_LINGER; //优雅关闭连接

    int sql_num;

    int thread_num;

    int close_log;

    int actor_model;
};
#endif