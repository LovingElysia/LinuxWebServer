//
// Created by elysia on 24-3-11.
//
#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H
#include<mysql/mysql.h>
#include<string>
#include"../lock/locker.h"
#include<list>
#include"../log/log.h"
class connection_pool
{
public:
    MYSQL* GetConnection();  //获取数据库连接·
    bool ReleaseConnection(MYSQL* mysql); //释放连接
    int GetFreeConn();     //获取连接
    void Destroy();   //销毁所有连接
    static connection_pool* GetInstance();  //单例模式
    void init(std::string url, std::string User, std::string PassWord, std::string DataBaseName, int Port, int MaxConn, int close_log);
private:
    connection_pool();
    ~connection_pool(){Destroy();}

    int m_MaxConn; //最大连接数
    int m_curconn; //当前使用的连接数
    int m_freeconn; //当前空闲的连接数
    locker m_lock;
    sem m_reserve;
    std::list<MYSQL*> connList;
public:
    std::string m_url; //主机地址
    std::string m_user; //数据库用户
    std::string m_passwd; //数据库密码
    std::string m_DataBaseName; //数据库名称
    std::string m_port; //端口号
    int m_close_log;  //日志开关
};

class connectionRAII
{
public:
    connectionRAII(MYSQL** conn,connection_pool* pool);
    ~connectionRAII();
private:
    MYSQL *conRAII;
    connection_pool* poolRAII;
};
#endif