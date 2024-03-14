//
// Created by elysia on 3/11/24.
//
#include "webserver.h"
WebServer::WebServer(int cap) : utils{cap}
{
    users = new http_conn[MAX_FD];

    // 初始化根目录
    char server_path[200];
    getcwd(server_path, 200);
    char root[8]{"../root"};
    m_root = new char[strlen(server_path)+strlen(root) + 1];
    strcpy(m_root, server_path);
    strcpy(m_root, root);

    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    delete[] users_timer;
    delete[] users;
    delete[] m_root;
    delete m_pool;
}

void WebServer::init(int port, std::string user, std::string PassWord, std::string DataBaseName, int log_wirte, int opt_linger, int trimode, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_passWord = PassWord;
    m_databaseName = DataBaseName;
    m_log_write = log_wirte;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trimode;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_close_log = close_log;
    m_actormodel = actor_model;
}

void WebServer::trig_mode()
{
    switch (m_TRIGMode)
    {
        case 0:
        {
            // LT + LT
            m_LISTENTrigmode = 0;
            m_CONNTrigmode = 0;
            break;
        }
        case 1:
        {
            // LT +ET
            m_LISTENTrigmode = 0;
            m_CONNTrigmode = 1;
            break;
        }
        case 2:
        {
            // ET + LT
            m_LISTENTrigmode = 1;
            m_CONNTrigmode = 0;
        }
        case 3:
        {
            // ET + ET
            m_LISTENTrigmode = 1;
            m_CONNTrigmode = 1;
            break;
        }
        default:
            break;
    }
}

void WebServer::log_write()
{
    if (0 == m_close_log)
    {
        // 初始化日志
        if (1 == m_log_write)LOG::get_instance()->init("./Serverlog", m_close_log, 2000, 80000, 800);
        else LOG::get_instance()->init("./Serverlog", m_close_log, 2000, 80000);
    }
}

void WebServer::sql_pool()
{
    m_connPool = connection_pool::GetInstance();
    std::string url("localhost");
    m_connPool->init(url, m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);
    users->initmysql_result(m_connPool);
}

void WebServer::thread_pool()
{
    m_pool = new pthreadpool<http_conn>(m_actormodel, m_connPool, m_thread_num, 10000);
}

void WebServer::eventListen()
{
    // 初始化监听socket
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);
    // 监听socket是否设置超时
    if (0 == m_OPT_LINGER)
    {
        struct linger tmp
                {
                        0, 1
                };
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (1 == m_OPT_LINGER)
    {
        struct linger tmp
                {
                        1, 1
                };
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    // 初始化服务器端地址socket
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(m_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // 设置地址socket可重用并绑定监听
    int flag{1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    int ret = bind(m_listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);
    // 初始化工具类
    utils.init(TIMESLOT);
    // 创建内核事件表
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    epoll_event events[MAX_EVENT_NUMBER];
    // 注册信号，文件描述符相关事件
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);

    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);
    Utils::u_epollfd = m_epollfd;
    Utils::u_pipefd = m_pipefd;
}
void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_TRIGMode, m_close_log, m_user, m_passWord, m_databaseName);
    users_timer[connfd].sockfd = connfd;
    users_timer[connfd].client_address = client_address;
    heap_timer *timer = new heap_timer{3 * TIMESLOT};
    timer->cb_func = cb_func;
    timer->user_data = &users_timer[connfd];
    users_timer[connfd].timer = timer;
    utils.m_time_heap.add_timer(timer);
}
void WebServer::adjust_timer(heap_timer *timer)
{
    time_t cur = time(nullptr);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_time_heap.adjust_timer(timer, 3 * TIMESLOT);
    LOG_INFO("%s", "adjust timer once");
}

void WebServer::deal_timer(heap_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
        utils.m_time_heap.del_timer(timer);
    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

bool WebServer::dealclientdata()
{
    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    if (0 == m_LISTENTrigmode)
    {
        int sockfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_len);
        if (sockfd < 0)
        {
            LOG_ERROR("%s:errno is: %d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_counter >= MAX_FD)
        {
            utils.show_error(sockfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(sockfd, client_address);
    }
    else
    {
        while (true)
        {
            int sockfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_len);
            if (sockfd < 0)
            {
                LOG_ERROR("%s:errno is: %d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_counter >= MAX_FD)
            {
                utils.show_error(sockfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(sockfd, client_address);
        }
        return false;
    }
    return true;
}

bool WebServer::dealwithsignal(bool &timeout, bool &server_stop)
{
    char signals[1024];
    int ret{0};

    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1 || ret == 0)
        return false;
    for (int i = 0; i < ret; ++i)
    {
        switch (signals[i])
        {
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                server_stop = true;
                break;
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd)
{
    heap_timer *timer = users_timer[sockfd].timer;
    // reactor
    if (1 == m_actormodel)
    {
        if (timer)
            adjust_timer(timer);
        m_pool->append(users + sockfd, 0);
        while (true)
        {
            if (users[sockfd].improv == 1)
            {
                if (users[sockfd].timer_flag == 1)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
        // proactor
    else
    {
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            m_pool->append_p(users + sockfd);
            if (timer)
                adjust_timer(timer);
        }
        else
            deal_timer(timer, sockfd);
    }
}

void WebServer::dealwithwrite(int sockfd)
{
    heap_timer *timer = users_timer[sockfd].timer;
    // reactor
    if (1 == m_actormodel)
    {
        if (timer)
            adjust_timer(timer);
        m_pool->append(users + sockfd, 1);
        while (true)
        {
            if (users[sockfd].improv == 1)
            {
                if (users[sockfd].timer_flag == 1)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr))
            if (timer)
                adjust_timer(timer);
        }
        else
            deal_timer(timer, sockfd);
    }
}
void WebServer::eventLoop()
{
    bool server_stop, timeout = false;
    // epoll监听事件
    while (!server_stop)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        for (int i = 0; i < number; ++i)
        {
            // 处理连接事件
            int sockfd = events[i].data.fd;
            if (sockfd == m_listenfd)
            {
                bool flag{dealclientdata()};
                if (!flag)
                    continue;
            }
            else if (events[i].events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP))
            {
                // 关闭连接
                deal_timer(users_timer[sockfd].timer, sockfd);
            }
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            { // 信号处理
                bool flag{dealwithsignal(timeout, server_stop)};
                if (!flag)
                    LOG_ERROR("%s", "dealwithsignal failure");
            }
            else if (events[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        if(timeout)
        {
            utils.timer_handler();

            LOG_INFO("%s","timer tick");

            timeout = false;
        }
    }
}
