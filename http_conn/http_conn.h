//
// Created by elysia on 24-3-11.
//
#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <strings.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../lock/locker.h"
#include <map>
#include <sys/epoll.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "../CGImysql/connection_pool.h"
class http_conn
{
public:
    constexpr static int FILENAME_LEN{200};
    constexpr static int READ_BUFSIZE{2048};
    constexpr static int WRITE_BUFSIZE{1024};
    enum METHOD     //http处理模块处理的方法
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    //主状态机对http报文处理的状态转移
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTE = 0,
        CHECK_STATE_HEAD,
        CHECK_STATE_CONTENT
    };
    //从状态机对请求行的处理状态转移
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_OPEN,
        LINE_BAD,
    };
    //对请求报文处理的结果
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
public:
    http_conn(){};
    ~http_conn(){};
public:
    void init(int sockfd,struct sockaddr_in& addr,char*,int,int,std::string user,std::string passwd,std::string sqlname);
    bool read_once();
    bool write();
    sockaddr_in* get_address(){return &m_address;}
    void close_conn(bool real_close = true);
    void process();
    void initmysql_result(connection_pool* pool);
private:
    void init();
    void unmap();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_request_head(char* text);
    HTTP_CODE parse_request_content(char* text);
    HTTP_CODE do_request();
    LINE_STATUS parse_line();
    char* getline(){return m_read_bufer+m_start_line;}
    bool add_status_line(int status,const char* title);
    bool add_header(int len);
    bool add_blank_line();
    bool add_content_lenth(int content_len);
    bool add_content_type();
    bool add_linger();
    bool add_content(const char* content);
    template <typename ...Args>
    bool add_response(const char* format,Args...args);
public:
    static inline int m_epollfd{0};
    static inline int m_user_counter{-1};
    int m_stat;
    MYSQL* mysql;
    int timer_flag;
    int improv;
private:
    sockaddr_in m_address;
    char m_read_bufer[READ_BUFSIZE];
    char m_write_bufer[WRITE_BUFSIZE];
    int m_start_line;
    long m_read_idx;  //从buffer中读取到的数据的长度
    long m_check_idx; //当前检查的字符
    int m_write_idx;

    char* m_url;
    char* m_version;
    char* m_host;
    char* m_string;
    METHOD m_method;
    CHECK_STATE m_check_state;
    bool m_linger;
    long m_content_lenth;
    int cgi;
    int m_sockfd;
    int m_TRIGMode;
    char* doc_root;
    char* m_file_address;
    char m_real_file[FILENAME_LEN];
    struct stat m_file_stat;
    std::map<std::string,std::string> m_users;
    struct iovec m_vector[2];
    int m_iv_count;
    int bytes_to_send;
    int bytes_have_send;
    char sql_user[100];
    char username[100];
    char passwd[100];
    int m_close_log;
};

template <typename ...Args>
bool http_conn::add_response(const char* format,Args...args)
{
    if(m_write_idx >= WRITE_BUFSIZE)return false;

    int len = std::snprintf(m_write_bufer+m_write_idx,WRITE_BUFSIZE-1-m_write_idx,format,args...);

    if(len > WRITE_BUFSIZE-1-m_write_idx)return false;

    m_write_idx += len;
    LOG_INFO("request %s",m_write_bufer);
    return true;
}
#endif
