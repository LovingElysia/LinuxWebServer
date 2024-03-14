//
// Created by elysia on 3/11/24.
//

#ifndef WEBSERVER_H
#define WEBSERVER_H
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string>
#include <arpa/inet.h>
#include "timer/time_heap.h"
#include "CGImysql/connection_pool.h"
#include "threadpool/pthreadpool.hpp"
#include "http_conn/http_conn.h"
#include "log/log.h"
#include <time.h>
#include <sys/time.h>
#include <cstring>
constexpr int MAX_FD{65536};
constexpr int MAX_EVENT_NUMBER{10000};
constexpr int TIMESLOT{5};

class WebServer
{
public:
    WebServer(int cap);
    ~WebServer();
public:
    void init(int port,std::string user,std::string PassWord,std::string DataBaseName,int log_wirte,int opt_linger,int trimode,int sql_num,int thread_num,int close_log,int actor_model);
    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(heap_timer *timer);
    void deal_timer(heap_timer *timer, int sockfd);
    bool dealclientdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);
private:
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;

    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    //数据库相关
    connection_pool *m_connPool;
    std::string m_user;         //登陆数据库用户名
    std::string m_passWord;     //登陆数据库密码
    std::string m_databaseName; //使用数据库名
    int m_sql_num;

    //线程池相关
    pthreadpool<http_conn> *m_pool;
    int m_thread_num;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    //定时器相关
    client_data *users_timer;
    Utils utils;
};
#endif