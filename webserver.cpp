#include "webserver.h"

webserver::webserver()
{
    users = new http_conn[MAX_FD];

    users_timer = new client_data[MAX_FD];
}

webserver::~webserver()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void webserver::init(int port, string user, string passWord, string databaseName, int log_write, 
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port=port;
    m_user=user;
    m_password=passWord;
    m_database_name=databaseName;
    m_mysql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_opt_linger = opt_linger;
    m_trigmode = trigmode;
    m_close_log = close_log;
    m_actormode = actor_model;
}

void webserver::trig_mode()
{
    switch (m_trigmode)
    {
    case 0:
        m_lisnten_trigmode=0;
        m_conn_trigmode=0;
        break;
    case 1:
        m_lisnten_trigmode=0;
        m_conn_trigmode=1;
        break;
    case 2:
        m_lisnten_trigmode=1;
        m_conn_trigmode=0;
        break;
    case 3:
        m_lisnten_trigmode=1;
        m_conn_trigmode=1;
        break;
    default:
        break;
    }
}

void webserver::log_write()
{
    //
}

void webserver::sql_pool()
{
    m_connpool = connection_pool::GetInstance();
    m_connpool->init("localhost", m_user, m_password, m_database_name, 3306,m_mysql_num);
    //users->init_mysql_result(m_connpool);
}

void webserver::thread_pool()
{
    m_pool = new threadpool<http_conn>(m_connpool, m_thread_num);
}

void webserver::event_listen()
{
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd>=0);

    if(m_opt_linger==0)
    {
        struct linger tmp={0,1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if(m_opt_linger==1)
    {
        struct linger tmp={1,1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    int ret=0;
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port=htonl(m_port);

    int flag=1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret=bind(m_listenfd, (struct sockaddr*)&addr, sizeof(addr));
    assert(ret>=0);
    ret=listen(m_listenfd, 5);
    assert(ret>=0);

    m_utils.init(TIMESLOT);

    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(10086);
    assert(m_epollfd!=-1);

    m_utils.add_fd(m_epollfd, m_listenfd, false, m_lisnten_trigmode);
    http_conn::m_epollfd = m_epollfd;

    ret=socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret!=-1);

    m_utils.setnonblocking(m_pipefd[1]);
    m_utils.add_fd(m_epollfd, m_pipefd[0], false, 0);

    m_utils.add_sig(SIGPIPE, SIG_IGN);
    m_utils.add_sig(SIGALRM, m_utils.sig_handler, false);
    m_utils.add_sig(SIGTERM, m_utils.sig_handler, false);

    alarm(TIMESLOT);

    utils::pipefd = m_pipefd;
    utils::u_epollfd = m_epollfd;
}

void webserver::timer(int connfd, struct sockaddr_in client_addr)
{
    users[connfd].init(connfd, client_addr, m_root, m_conn_trigmode, 
        m_close_log, m_user, m_password, m_database_name);
    
    users_timer[connfd].address = client_addr;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur+3*TIMESLOT;
    users_timer[connfd].timer = timer;
    m_utils.timer_lst.add_timer(timer);
}

void webserver::adjust_timer(util_timer* timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3*TIMESLOT;
    m_utils.timer_lst.adjust_timer(timer);

    //TODO: LOG
}

void webserver::deal_timer(util_timer* timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if(timer)
    {
        m_utils.timer_lst.del_timer(timer);
    }
}

bool webserver::dealclientdata()
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    if(m_lisnten_trigmode==0)
    {
        int connfd = accept(m_listenfd, (struct sockaddr*)&client_addr, &client_addr_len);
        if(connfd<0)
        {
            return false;
        }
        if(http_conn::m_user_count>=MAX_FD)
        {
            return false;
        }
        timer(connfd, client_addr);
    }
    else
    {
        while(1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr*)&client_addr, &client_addr_len);
            if(connfd<0)
            {
                break;
            }   
            if(http_conn::m_user_count>=MAX_FD)
            {
                break;
            }
            timer(connfd, client_addr);
        }
        return false;
    }
    return true;
}

bool webserver::dealwithsignal(bool &timeout, bool &stopserver)
{
    int ret=0;
    int sig=0;
    char signal[1024];
    ret = recv(m_pipefd[0], signal, sizeof(signal), 0);
    if(ret==-1)
    {
        return false;
    }
    else if(ret==0)
    {
        return false;
    }
    else
    {
        for(int i=0;i<ret;i++)
        {
            if(signal[i]==SIGALRM)
            {
                timeout=true;
                break;
            }
            else if(signal[i]==SIGTERM)
            {
                stopserver=true;
                break;
            }
        }
    }
    return true;
}

void webserver::dealwithread(int sockfd)
{
    util_timer* timer = users_timer[sockfd].timer;

    if(m_actormode==1)
    {
        if(timer)
        {
            adjust_timer(timer);
        }
        m_pool->append(users+sockfd, 0);
        while(true)
        {
            if(users[sockfd].improv==1)
            {
                if(users[sockfd].timer_flag==1)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag=0;
                }
                users[sockfd].improv=0;
                break;
            }
        }
    }
    else
    {
        if(users[sockfd].read_once())
        {
            m_pool->append_p(users+sockfd);
            if(timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}


void webserver::dealwithwrite(int sockfd)
{
    util_timer* timer = users_timer[sockfd].timer;

    if(m_actormode==1)
    {
        if(timer)
        {
            adjust_timer(timer);
        }
        m_pool->append(users+sockfd, 1);
        while(true)
        {
            if(users[sockfd].improv==1)
            {
                if(users[sockfd].timer_flag==1)
                {
                    deal_timer(timer,sockfd);
                    users[sockfd].timer_flag=0;
                }
                users[sockfd].improv=0;
                break;
            }
        }
    }
    else
    {
        if(users[sockfd].write())
        {
            if(timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void webserver::event_loop()
{
    bool timeout=false;
    bool stopserver=false;
    while(!stopserver)
    {
        int num = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(num<0&&errno!=EINTR)
        {
            break;
        }
        for(int i=0;i<num;i++)
        {
            int sockfd=events[i].data.fd;
            if(sockfd==m_listenfd)
            {
                if(!dealclientdata())
                {
                    continue;
                }
            }
            else if(events[i].events & (EPOLLRDHUP|EPOLLHUP|EPOLLERR))
            {
                util_timer* timer = users_timer[sockfd].timer;
                deal_timer(timer,sockfd);
            }
            else if(events[sockfd].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if((sockfd==m_pipefd[0])&&(events[sockfd].events & EPOLLIN)) 
            {
                if(!dealwithsignal(timeout, stopserver))
                {
                    //TODO: LOG
                }   
            }
            else if(events[sockfd].events&EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        if(timeout)
        {
            m_utils.timer_handler();
            timeout=false;
        }
    }
}