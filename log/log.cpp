//
// Created by elysia on 24-3-11.
//
#include "log.h"

LOG::LOG() : m_count{0}, m_is_async{false} {}

LOG::~LOG()
{
    if (!m_fp)
        fclose(m_fp);
}

bool LOG::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    if (max_queue_size >= 1)
    {
        m_is_async = true;
        m_log_queue = new block_queue<std::string>{max_queue_size};
        pthread_t tid;
        pthread_create(&tid, nullptr, flush_log_thread, nullptr);
    }

    // 初始化成员,分配缓冲区内存
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    m_split_lines = split_lines;

    // 获取系统时
    time_t t;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/');
    char log_full_name[300] = {0};
    if (!p)
    {
        std::snprintf(log_full_name, sizeof (log_full_name), "%d_%02d_%02d_%s", my_tm.tm_year, my_tm.tm_mon, my_tm.tm_mday, file_name);
    }
    else
    {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        std::snprintf(log_full_name, sizeof(log_full_name), "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year, my_tm.tm_mon, my_tm.tm_mday, log_name);
    }
    m_today = my_tm.tm_mday;
    m_fp = fopen(log_full_name, "a");
    if (!m_fp)
        return false;
    return true;
}

void LOG::write_log(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level)
    {
        case 0:
        {
            strcpy(s, "[debug]:");
            break;
        }
        case 1:
        {
            strcpy(s, "[info]:");
            break;
        }
        case 2:
        {
            strcpy(s, "[warn]:");
            break;
        }
        case 3:
        {
            strcpy(s, "[error]:");
            break;
        }
        default:
        {
            strcpy(s, "[info]:");
            break;
        }
    }
    m_mutex.lock();
    m_count++;
    if ((m_today != my_tm.tm_mday) || (m_count % m_split_lines == 0))
    {
        char new_log[300] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};
        snprintf(tail, 15, "%d_%02d_%02d", my_tm.tm_year, my_tm.tm_mon, my_tm.tm_mday);
        if (m_today != my_tm.tm_mday)
        {
            std::snprintf(new_log, sizeof(new_log), "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        std::snprintf(new_log, sizeof(new_log), "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        m_fp = fopen(new_log, "a");
    }
    m_mutex.unlock();

    va_list va_lst;
    va_start(va_lst, format);
    m_mutex.lock();
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d:%06ld %s", my_tm.tm_year, my_tm.tm_mon, my_tm.tm_mday, my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, va_lst);
    m_buf[m + n] = '\r';
    m_buf[m + n + 1] = '\0';
    std::string log_str{m_buf};
    m_mutex.unlock();

    if (m_is_async && !m_log_queue->full())
    {
        m_log_queue->push_back(log_str);
    }
    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
    va_end(va_lst);
}

void LOG::flush()
{
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}

void *LOG::async_write_log()
{
    std::string single_log;
    while (m_log_queue->pop_front(single_log))
    {
        m_mutex.lock();
        fputs(single_log.c_str(), m_fp);
        m_mutex.unlock();
    }
    return nullptr;
}