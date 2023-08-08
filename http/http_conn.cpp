#include"http_conn.h"
#include<mysql/mysql.h>
#include<map>
#include<fstream>
#include<iostream>

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;
char* write_content_buffer;
int write_content_buffer_len=0;

/// @brief 将文件描述符设置为非阻塞
/// @param fd: 文件描述符
/// @return 描述符原来的状态
int set_non_block(int fd)
{
    int old = fcntl(fd, F_GETFL);
    int new_op = old | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_op);
    return old;
}

/// @brief 从内核事件表注册读事件
/// @param epollfd 
/// @param fd 
/// @param oneshot 
/// @param TRIGMode 1为ET模式
void addfd(int epollfd, int fd, bool oneshot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    if(TRIGMode==1)
    {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if(oneshot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_non_block(fd);
}

/// @brief 从内核事件表删除描述符
/// @param epollfd 
/// @param fd 
void remove_fd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

/// @brief 将fd的模式修改为oneshot
/// @param epollfd 
/// @param fd 
/// @param ev 
/// @param TRIGMode 
void set_epoll_oneshot(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd=fd;
    if(TRIGMode==1)
    {
        event.events = ev | EPOLLONESHOT | EPOLLET | EPOLLRDHUP;
    }
    else
    {
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

/// @brief 关闭链接
/// @param close 是否关闭 
void http_conn::close_conn(bool close)
{
    if(close&&m_sockfd!=-1)
    {
        //TODO: log close fd

        remove_fd(m_epollfd, m_sockfd);
        m_sockfd=-1;
        m_user_count--;
    }
}

/// @brief 外部用来初始化socket链接的函数
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname)
{
    write_content_buffer = new char[WRITE_BUFFER_CONTENT_SIZE];
    memset(write_content_buffer, '\0', WRITE_BUFFER_CONTENT_SIZE);
    m_sockfd = sockfd;
    m_sockaddr = addr;
    //作者项目这个在fd后面，逻辑不合适？
    m_TRIGMode=TRIGMode;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);

    m_user_count++;
    doc_root = root;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

/// @brief 初始化链接数据
void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    byte_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_check_index = 0;
    m_read_index = 0;
    m_write_index = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buffer, '\0', READ_BUFFER_SIZE);
    memset(m_write_buffer, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FIlENAME_LEN);
}

/// @brief 分析出一行数据在readbuffer里面，checkindex指向下一行开头
/// @return 状态：LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINESTATUS http_conn::parse_line()
{
    char temp;
    for(;m_check_index<m_read_index;m_check_index++)
    {
        temp=m_read_buffer[m_check_index];
        if(temp=='\r')
        {
            if(m_check_index+1==m_read_index)
            {
                return LINE_OPEN;
            }
            else if(m_read_buffer[m_check_index+1]=='\n')
            {
                m_read_buffer[m_check_index++]='\0';
                m_read_buffer[m_check_index++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp=='\n')
        {
            if(m_check_index>1&&m_read_buffer[m_check_index-1]=='\r')
            {
                m_read_buffer[m_check_index++]='\0';
                m_read_buffer[m_check_index++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

/// @brief 读取客户端数据
/// @return 
bool http_conn::read_once()
{
    if(m_read_index>=READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;
    //LT
    if(m_TRIGMode==0)
    {
        bytes_read = recv(m_sockfd, m_read_buffer+m_read_index,READ_BUFFER_SIZE-m_read_index,0);
        if(byte_have_send<=0)
        {
            return false;
        }
        m_read_index+=bytes_read;
        return true;
    }
    else//ET
    {
        while(true)
        {
            bytes_read = recv(m_sockfd, m_read_buffer+m_read_index,READ_BUFFER_SIZE-m_read_index,0);
            if(bytes_read==-1)
            {
                if(errno == EAGAIN||errno==EWOULDBLOCK)
                {
                    break;
                }
                return false;
            }
            else if(bytes_read==0)
            {
                return false;
            }
            m_read_index += bytes_read;
        }
        return true;
    }
}

/// @brief 解析http请求行，获得请求方法，目标url及http版本号
/// @param text 
/// @return 状态
http_conn::HTTPCODE http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text," \t");
    if(!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char* method = text;
    if(strcmp(method,"GET")==0)
    {
        m_method=GET;
    }
    else if(strcmp(method,"POST")==0)
    {
        m_method=POST;
    }
    else
    {
        return BAD_REQUEST;
    }
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if(!m_version)
    {
        return BAD_REQUEST;
    }
    *m_version++='\0';
    m_version+=strspn(m_version," \t");
    if(strcasecmp(m_version, "HTTP/1.1")!=0)
    {
        return BAD_REQUEST;
    }
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    if (strlen(m_url) == 1)
        strcat(m_url, "init");
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

/// @brief 解析http请求的一个头部信息
/// @param text 
/// @return 状态
http_conn::HTTPCODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        //没有头部数据
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        //TODO: log error
    }
    return NO_REQUEST;
}

/// @brief 判断http请求是否被完整读入，将text存进m_string中
/// @param text 
/// @return 读取状态
http_conn::HTTPCODE http_conn::parse_content(char *text)
{
    if (m_read_index >= (m_content_length + m_check_index))
    {
        text[m_content_length] = '\0';
        //content原文
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTPCODE http_conn::process_read()
{
    std::cout<<"enter process read"<<std::endl;
    LINESTATUS linestatus = LINE_OK;
    HTTPCODE httpcode = NO_REQUEST;
    char* text = 0;
    while((m_check_state == CHECK_STATE_CONTENT && linestatus == LINE_OK)||
    ((linestatus = parse_line())==LINE_OK))
    {
        text = get_line();
        m_start_line = m_check_index;
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
            httpcode = parse_request_line(text);
            if(httpcode==BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        case CHECK_STATE_HEADER:
            httpcode = parse_headers(text);
            if(httpcode==BAD_REQUEST)
            {
                return httpcode;
            }
            else if(httpcode == GET_REQUEST)
            {
                return do_request();
            }
            break;
        case CHECK_STATE_CONTENT:
            httpcode = parse_content(text);
            if(httpcode==GET_REQUEST)
            {
                return do_request();
            }
            linestatus = LINE_OPEN;
            break;
        default:
            return INTERNAL_ERROR;
            break;
        }
    }
    return NO_REQUEST;
}
bool http_conn::write()
{
    std::cout<<"enter process write"<<std::endl;
    int temp=0;
    if(bytes_to_send==0)
    {
        set_epoll_oneshot(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }
    while(1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if(temp<0)
        {
            if(errno==EAGAIN)
            {
                set_epoll_oneshot(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            return false;
        }
        byte_have_send += temp;
        bytes_to_send -= temp;
        if(bytes_to_send>=m_iv[0].iov_len)
        {
            m_iv[0].iov_len=0;
            m_iv[1].iov_len=bytes_to_send;
            m_iv[1].iov_base=write_content_buffer+(byte_have_send-m_write_index);
        }
        else
        {
            m_iv[0].iov_base = m_write_buffer+byte_have_send;
            m_iv[0].iov_len=m_iv[0].iov_len-byte_have_send;
        }
        if(byte_have_send<=0)
        {
            set_epoll_oneshot(m_epollfd, m_sockfd,  EPOLLIN, m_TRIGMode);
            if(m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    return false;
}

bool http_conn::add_response(const char *format, ...)
{
    if (m_write_index >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buffer + m_write_index, WRITE_BUFFER_SIZE - 1 - m_write_index, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_index))
    {
        va_end(arg_list);
        return false;
    }
    m_write_index += len;
    va_end(arg_list);
    //LOG_INFO("request:%s", m_write_buf);
    return true;
}

bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}

bool http_conn::process_write(HTTPCODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if(!add_content(error_500_form))
        {
            return false;
        }
        break;
    case BAD_REQUEST:
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if(!add_content(error_404_form))
        {
            return false;
        }
        break;
    case FORBIDDEN_REQUEST:
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if(!add_content(error_403_form))
        {
            return false;
        }
        break;
    case FILE_REQUEST:
        add_status_line(200, ok_200_title);
        add_headers(write_content_buffer_len);
        m_iv[0].iov_base = m_write_buffer;
        m_iv[0].iov_len = m_write_index;
        m_iv[1].iov_base = write_content_buffer;
        m_iv[1].iov_len = write_content_buffer_len;
        m_iv_count=2;
        bytes_to_send=m_write_index+write_content_buffer_len;
        break;
    default:
    return false;
        break;
    }
    m_iv[0].iov_base = m_write_buffer;
    m_iv[0].iov_len = m_write_index;
    m_iv_count=1;
    bytes_to_send=m_write_index;
    return true;
}

void http_conn::process()
{
    std::cout<<"enter process"<<std::endl;
    HTTPCODE ret=process_read();
    if(ret==NO_REQUEST)
    {
        set_epoll_oneshot(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    bool wret = process_write(ret);
    if(!wret)
    {
        close_conn();
    }
    set_epoll_oneshot(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}

/// @brief 
/// @return 返回状态
http_conn::HTTPCODE http_conn::do_request()
{
    const char *p = strrchr(m_url, '/');
    //TODO: 根据url去搜索数据库提供相关服务

    //数据存入write_content_buffer里面并且设置write_content_buffer_len长度
    //请求行、头部写入m_write_buffer
    char request_url[1024];
    memcpy(request_url, p, sizeof(p));
    std::cout<<request_url<<std::endl;
    if(strcmp(request_url, "\test")==0)
    {
        char a[1024] = "{\"data\":\"hello world\"}";
        strncat(write_content_buffer, a, sizeof(a));
        write_content_buffer_len = strlen(write_content_buffer);
    }
    return FILE_REQUEST;
}
