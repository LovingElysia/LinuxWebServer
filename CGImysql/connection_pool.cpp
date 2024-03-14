//
// Created by elysia on 24-3-11.
//
#include "connection_pool.h"

connection_pool *connection_pool::GetInstance()
{
    static connection_pool pool;
    return &pool;
}

connection_pool::connection_pool() : m_curconn{0}, m_freeconn{0} {}

void connection_pool::init(std::string url, std::string User, std::string PassWord, std::string DataBaseName, int Port, int MaxConn, int close_log)
{
    m_url = url;
    m_user = User;
    m_passwd = PassWord;
    m_DataBaseName = DataBaseName;
    m_port = Port;
    m_close_log = close_log;
    for (int i = 0; i < MaxConn; ++i)
    {
        MYSQL *conn{nullptr};
        conn = mysql_init(conn);
        if (!conn)
        {
            LOG_ERROR("MYSQL ERROR");
            exit(1);
        }
        conn = mysql_real_connect(conn, m_url.c_str(), m_user.c_str(), m_passwd.c_str(), m_DataBaseName.c_str(), Port, nullptr, 0);
        if (!conn)
        {
            LOG_ERROR("MySQL Error");
            exit(1);
        }
        connList.push_back(conn);
        ++m_freeconn;
    }
    m_reserve = sem(m_freeconn);
    m_MaxConn = m_freeconn;
}

MYSQL *connection_pool::GetConnection()
{
    MYSQL *conn{nullptr};
    if (connList.size() == 0)
        return nullptr;

    m_reserve.wait();
    m_lock.lock();
    conn = connList.front();
    connList.pop_front();
    --m_freeconn;
    ++m_curconn;
    m_lock.unlock();
    return conn;
}

bool connection_pool::ReleaseConnection(MYSQL *con)
{
    if (!con)
        return false;
    m_lock.lock();
    connList.push_back(con);
    ++m_freeconn;
    --m_curconn;
    m_lock.unlock();
    m_reserve.post();
    return true;
}
void connection_pool::Destroy()
{
    m_lock.lock();
    if (connList.size() > 0)
    {
        for (auto it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *con{*it};
            mysql_close(con);
        }
        m_curconn = 0;
        m_freeconn = 0;
        connList.clear();
    }
    m_lock.unlock();
}

int connection_pool::GetFreeConn()
{
    return m_curconn;
}

connectionRAII::connectionRAII(MYSQL** SQL,connection_pool* pool)
{
    *SQL = pool->GetConnection();
    conRAII = *SQL;
    poolRAII = pool;
}
connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}