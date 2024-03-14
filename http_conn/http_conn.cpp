//
// Created by elysia on 24-3-11.
//
#include"http_conn.h"
#include"../log/log.h"
constexpr const char* ok_200_title{"OK"};
constexpr const char* error_400_title{"Bad Request"};
constexpr const char* error_400_form{"Your request has bad syntax or is inherently impossible to staisfy.\n"};
constexpr const char* error_403_title{"Forbidden"};
constexpr const char* error_403_form{"You do not have permission to get file form this server.\n"};
constexpr const char* error_404_title{"Not Found"};
constexpr const char* error_404_form{"The requested file was not found on this server.\n"};
constexpr const char* error_500_title{"Internal Error"};
constexpr const char* error_500_form{"There was an unusual problem serving the request file.\n"};
locker m_lock;
std::map<std::string,std::string> users;
int setnonblocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}
void addfd(int epollfd,int fd,bool one_shot,int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    if(1==TRIGMode)event.events = EPOLLIN|EPOLLET|EPOLLRDHUP;
    event.events = EPOLLIN|EPOLLRDHUP;
    if(one_shot)event.events|=EPOLLONESHOT;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}
void removefd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}
void modfd(int epollfd,int fd,int ev,int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    if(1==TRIGMode)event.events = ev|EPOLLRDHUP|EPOLLET|EPOLLONESHOT;
    event.events = ev|EPOLLRDHUP|EPOLLONESHOT;

    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}
//从状态机 分析http报文的一行
http_conn::LINE_STATUS http_conn::parse_line()
{
    char tmp;
    for(;m_check_idx<m_read_idx;++m_check_idx)
    {
        tmp = m_read_bufer[m_check_idx];
        if(tmp == '\r')
        {
            if(m_check_idx+1 == m_read_idx)
                return LINE_OPEN;
                //若下一个字符是'\n',则解析完一行
            else if(m_read_bufer[m_check_idx+1]=='\n')
            {
                m_read_bufer[m_check_idx++] = '\0';
                m_read_bufer[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        if(tmp == '\n')
        {
            // 若上一个字符为'\r',则说明解析完一行
            if((m_check_idx>1)&&(m_read_bufer[m_check_idx-1]=='\r'))
            {
                m_read_bufer[m_check_idx-1] = '\0';
                m_read_bufer[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;  //否则需要读取更多内容
}

// 主状态机解析请求行 目标请求方法 目标url http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char* text)
{
    //请求行的各元素之间以空格隔开 查找其中是否有空格或'\t'字符
    m_url = strpbrk(text," \t");
    if(!m_url)
        return BAD_REQUEST;
    *m_url ++ = '\0';//将方法的结尾标记'\0'
    char *method = text;
    if(strcasecmp(method,"GET")==0)
        m_method = GET;
    else if(strcasecmp(method,"POST")==0)
    {
        m_method = POST;
        cgi = 1;
    }
    else return BAD_REQUEST; //否则说明这是一个错误的请求报文

    //防止多个空格
    m_url+=strspn(text,"\t"); //此时m_url已指向url首字符
    //同样的方式获取版本号
    m_version = strpbrk(m_url," \t")
            ;if(!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version," \t");
    if(strcasecmp(m_version,"HTTP/1.1")!=0)
        return BAD_REQUEST;

    //防止某些url带有http://
    if(strncasecmp(m_url,"http://",7)==0)
    {
        m_url+=7;
        m_url = strchr(m_url,'/');
    }
    //对https做相同处理
    if(strncasecmp(m_url,"https://",8)==0)
    {
        m_url+=8;
        m_url = strchr(m_url,'/');
    }
    if((!m_url)||(m_url[0]!='/'))
        return BAD_REQUEST;
    //如果url为'/'显示判断界面
    if(strlen(m_url)==1)
        strcat(m_url,"judge.html");
    m_check_state = CHECK_STATE_HEAD;
    return NO_REQUEST;
}

//解析http头部请求
http_conn::HTTP_CODE http_conn::parse_request_head(char* text)
{
    //判断是否是空行 //头部字段和content由'\r\n'分隔 从状态机将每行末尾'\r\n'置为'\0'
    if(text[0]=='\0')
    {
        //若content-length字段不为0,说明这是多次解析头部字段后来到头部字段末尾 并且是POST请求
        if(m_content_lenth!=0)
        {
            m_check_state = CHECK_STATE_CONTENT; //主状态解析报文内容
            return NO_REQUEST;
        }
        return GET_REQUEST;  //否则说明是GET请求,GET请求无conten部分,content-lenth为0
    }
        //只处理content-lenth字段和connection字段
    else if(strncasecmp(text,"Connection:",11)==0)
    {
        text += 11;
        text += strspn(text," \t");
        if(strcasecmp(text,"keep-alive")==0)
        {
            m_linger = true;
        }
    }
    else if(strncasecmp(text,"Content-length:",15)==0)
    {
        text += 15;
        text += strspn(text," \t");
        m_content_lenth = atol(text);
    }
    else if(strncasecmp(text,"Host:",5)==0)
    {
        text += 5;
        text += strspn(text," \t");
        m_host = text;
    }
    else
    {
        LOG_INFO("oop! unknow header: %s",text);
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_request_content(char* text)
{
    //判断http报文是否读取完整
    if(m_read_idx >= m_content_lenth+m_check_idx)
    {
        text[m_content_lenth] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text{nullptr};
    while((m_check_state==CHECK_STATE_CONTENT)&&(line_status==LINE_OK)||((line_status=parse_line())==LINE_OK))
    {
        text = getline();
        m_start_line = m_check_idx;
        LOG_INFO("%s",text);
        switch(m_check_state)
        {
            case CHECK_STATE_REQUESTE:
            {
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST) return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEAD:
            {
                ret = parse_request_head(text);
                if(ret == BAD_REQUEST) return BAD_REQUEST;
                else if(ret==GET_REQUEST) return do_request();
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret = parse_request_content(text);
                if(ret == GET_REQUEST) return do_request();
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

bool http_conn::read_once()
{
    if(m_read_idx >= READ_BUFSIZE)return false;
    int read_bytes{0};
    if(1==m_TRIGMode)
    {
        //电平模式读取数据
        read_bytes = recv(m_sockfd,m_read_bufer+m_read_idx,READ_BUFSIZE-m_read_idx,0);
        m_read_idx += read_bytes;
        if(read_bytes<=0) return false;
        return true;
    }
    else
    {
        while(true)
        {
            read_bytes = recv(m_sockfd,m_read_bufer+m_read_idx,READ_BUFSIZE-m_read_idx,0);
            if(read_bytes==-1)
            {
                if(errno==EAGAIN||errno==EWOULDBLOCK) break;
                return false;
            }
            else if(read_bytes==0)return false;
            m_read_idx += read_bytes;
        }
    }
    return true;
}

http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file,doc_root);
    int len = strlen(doc_root);

    //找到url中'/'最后出现的位置
    const char* p = strrchr(m_url,'/');
    if((cgi==1)&&((*(p+1)=='2')||(*(p+1)=='3')))
    {
        //登录检验或注册检验
        char* m_url_real = new char[FILENAME_LEN];
        strcpy(m_url_real,"/");
        strcat(m_url_real,m_url+2);
        strncpy(m_real_file+len,m_url_real,FILENAME_LEN-len-1);
        delete[]m_url_real;

        char name[100],passwd[100];
        int i;
        //user=yds&password=123
        for(i=5;m_string[i]!='&';++i)name[i-5]= m_string[i];
        name[i-5]='\0';
        int j;
        for(i=i+10;m_string[i]!='\0';++i,++j)passwd[j]=m_string[i];
        passwd[j]='\0';

        //如果是注册检验 先检测是否有重名,没有重名就进行注册
        if(*(p+1)=='3')
        {
            if(users.find(name)==users.end())
            {

                char* sql_insert = new char[FILENAME_LEN];
                sprintf(sql_insert,"INSERT INTO user_info(username,passwd) VALUES('%s','%s')",name,passwd);
                m_lock.lock();
                int res = mysql_query(mysql,sql_insert);
                users.insert(std::pair<std::string,std::string>(name,passwd));
                m_lock.unlock();
                if(!res)strcpy(m_url,"/log.html");
                else strcpy(m_url,"/registerError.html");
            }
            else strcpy(m_url,"/registerError.html");
        }
        //否则就是登录 进行登录判断
        else if((users.find(name)!=users.end()) && users[name] == passwd) strcpy(m_url,"/welcome.html");
        else strcpy(m_url,"/logError.html");
    }
    if(*(p+1)=='0')
    {
        char* m_url_real = new char[FILENAME_LEN];
        strcpy(m_url_real,"/register.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));
        delete []m_url_real;
    }
    else if(*(p+1)=='1')
    {
        char* m_url_real = new char[FILENAME_LEN];
        strcpy(m_url_real,"/log.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));
        delete []m_url_real;
    }
    else if(*(p+1)=='5')
    {
        char* m_url_real = new char[FILENAME_LEN];
        strcpy(m_url_real,"/picture.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));
        delete []m_url_real;
    }
    else if(*(p+1)=='6')
    {
        char* m_rul_real = new char[FILENAME_LEN];
        strcpy(m_rul_real,"/video.html");
        strncpy(m_real_file+len,m_rul_real,strlen(m_rul_real));
        delete []m_rul_real;
    }
    else if(*(p+1)=='7')
    {
        char* m_url_real = new char[FILENAME_LEN];
        strcpy(m_url_real,"/fans.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));
        delete []m_url_real;
    }
    else strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);
    if(stat(m_real_file,&m_file_stat)<0)return NO_RESOURCE;
    if(!(m_file_stat.st_mode&S_IROTH))return FORBIDDEN_REQUEST;
    if(S_ISDIR(m_file_stat.st_mode))return BAD_REQUEST;

    int fd{open(m_real_file,O_RDONLY)};
    m_file_address = (char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);
    return FILE_REQUEST;
}
void http_conn::close_conn(bool real_close)
{
    if(real_close && m_sockfd!=-1)
    {
        printf("close %d\n",m_sockfd);
        removefd(m_epollfd,m_sockfd);
        m_sockfd = -1;
        m_user_counter --;
    }
}
bool http_conn::add_content_lenth(int content_len)
{
    return add_response("Content-Length:%d\r\n",content_len);
}
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n","text/html");
}
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n",m_linger?"keep-alive":"close");
}
bool http_conn::add_blank_line()
{
    return add_response("%s","\r\n");
}
bool http_conn::add_header(int len)
{
    return add_content_lenth(len)&&add_linger()&&add_blank_line();
}
bool http_conn::add_status_line(int status,const char* title)
{
    return add_response("%s %d %s","HTTP/1.1",status,title);
}
bool http_conn::add_content(const char* content)
{
    return add_response("%s",content);
}
bool http_conn::process_write(http_conn::HTTP_CODE ret)
{
    switch(ret)
    {
        case INTERNAL_ERROR:
        {
            add_status_line(500,error_500_title);
            add_header(strlen(error_500_form));
            if(!add_content(error_500_form))return false;
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(404,error_404_title);
            add_header(strlen(error_404_form));
            if(!add_content(error_404_form))return false;
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(403,error_403_title);
            add_header(strlen(error_403_form));
            if(!add_content(error_403_form))return false;
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200,ok_200_title);
            if(m_file_stat.st_size!=0)
            {
                add_header(m_file_stat.st_size);
                m_vector[0].iov_base = m_write_bufer;
                m_vector[0].iov_len = m_write_idx;
                m_vector[1].iov_base = m_file_address;
                m_vector[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
            else
            {
                const char* ok_string{"<html><body></body></html>"};
                add_header(strlen(ok_string));
                if(!add_content(ok_string))return false;
            }
        }
        default:
            break;
    }
    m_vector[0].iov_base = m_write_bufer;
    m_vector[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}
void http_conn::init()
{
    mysql = nullptr;
    bytes_have_send = 0;
    bytes_to_send = 0;
    m_check_state = CHECK_STATE_REQUESTE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_lenth = 0;
    m_host = 0;
    m_start_line = 0;
    m_check_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_stat = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_bufer, '\0', READ_BUFSIZE);
    memset(m_write_bufer, '\0', WRITE_BUFSIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}
void http_conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address = nullptr;
    }
}
bool http_conn::write()
{
    int tmp{0};
    //若要发送的数据为空
    if(bytes_to_send==0)
    {
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
        init();
        return true;
    }
    while(true)
    {
        tmp = writev(m_sockfd,m_vector,m_iv_count);
        if(tmp<0)
        {
            if(errno==EAGAIN)
            {
                modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += tmp;
        bytes_to_send -=tmp;

        if(bytes_have_send>=m_vector[0].iov_len)
        {
            m_vector[0].iov_len = 0;
            m_vector[1].iov_base = m_file_address + (bytes_have_send-m_write_idx);
            m_vector[1].iov_len = bytes_to_send;
        }
        else
        {
            m_vector[0].iov_base = m_write_bufer + bytes_have_send;
            m_vector[0].iov_len = m_vector[0].iov_len - bytes_have_send;
        }

        if(bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
            if(m_linger)
            {
                init();
                return true;
            }
            return false;
        }
    }
}
void http_conn::process()
{
    HTTP_CODE ret{process_read()};
    if(ret == NO_REQUEST)
    {
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
        return;
    }

    int write_ret{process_write(ret)};
    if(!write_ret)close_conn();
    modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TRIGMode);
}
void http_conn::init(int sockfd,sockaddr_in& addr,char* root,int TRIGMode,int close_log,std::string user,std::string passwd,std::string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;
    addfd(m_epollfd,sockfd,true,m_TRIGMode);
    m_user_counter++;

    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(username,user.c_str());
    strcpy(this->passwd,passwd.c_str());
    strcpy(sql_user,sqlname.c_str());

    init();
}
void http_conn::initmysql_result(connection_pool* pool)
{
    MYSQL* mysql = {nullptr};
    connectionRAII mysqlcon{&mysql,pool};
    if(mysql_query(mysql,"select username,passwd from user_info"))
    {
        LOG_ERROR("select error: %s",mysql_error(mysql));
    }
    MYSQL_RES* result{mysql_store_result(mysql)};
    while(auto row{mysql_fetch_row(result)})
    {
        std::string tmp1{row[0]};
        std::string tmp2{row[1]};
        users[tmp1] = tmp2;
    }
}