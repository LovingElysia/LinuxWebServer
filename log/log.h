#ifndef LOG_H
#define LOG_H
#include <stdarg.h>
#include <iostream>
#include "block_queue.hpp"
#include <cstring>
#include <time.h>
#include <sys/time.h>

class LOG{

public:
    static LOG* get_instance()
    {
        static  LOG instance;
        return &instance;
    }
    static void *flush_log_thread(void *args)
    {
        LOG::get_instance()->async_write_log();
        return nullptr;
    }
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
    void write_log(int level,const char* format,...);
    void flush(void);

private:
    LOG();
    virtual ~LOG();
    void* async_write_log();
private:
    char dir_name[128];   //路径名
    char log_name[128];   //日志文件名
    int m_split_lines;    //日志最大行数
    int m_log_buf_size;   //日志缓冲区大小
    int m_today;          //按天分类,记录当天时间
    FILE* m_fp;           //日志文件指针
    char* m_buf;
    block_queue<std::string>* m_log_queue;
    bool m_is_async;
    locker m_mutex;
    int m_close_log;
    long long m_count;
};
#define LOG_DEBUG(format, ...) if(0 == m_close_log) {LOG::get_instance()->write_log(0, format, ##__VA_ARGS__); LOG::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {LOG::get_instance()->write_log(1, format, ##__VA_ARGS__); LOG::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {LOG::get_instance()->write_log(2, format, ##__VA_ARGS__); LOG::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {LOG::get_instance()->write_log(3, format, ##__VA_ARGS__); LOG::get_instance()->flush();}
#endif