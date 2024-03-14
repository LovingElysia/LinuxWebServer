//
// Created by elysia on 3/11/24.
//
#include "config.h"
#include <iostream>
int main(int argc,char* argv[])
{
    std::string user{"root"};
    std::string passwd{"xwyjzjdzd2233"};
    std::string databasename = "user";

    //命令行解析
    Config config;
    config.parse_args(argc, argv);

    WebServer server{50000};

    //初始化
    server.init(config.port, user, passwd, databasename, config.LOGWrite,
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num,
                config.close_log, config.actor_model);


    //日志
    server.log_write();

    //数据库
    server.sql_pool();

    //线程池
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}