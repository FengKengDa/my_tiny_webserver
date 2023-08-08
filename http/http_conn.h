#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>


#include"../lock/locker.h"
#include"../mysqlpool/sql_connection_pool.h"

using std::map;
using std::string;

#define FIlENAME_LEN 200
#define READ_BUFFER_SIZE 2048
#define WRITE_BUFFER_SIZE 1024
#define WRITE_BUFFER_CONTENT_SIZE 1024*1024

class http_conn
{
public:
    enum METHOD
    {
        GET, 
        POST
    };
    enum CHECKSTATE
    {
        CHECK_STATE_REQUESTLINE,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTPCODE
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
    enum LINESTATUS
    {
        LINE_OK,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn(){};
    ~http_conn(){};
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    void close_conn(bool real_colse=true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in* get_address(){return &m_sockaddr;}

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL* mysql;
    int m_state;
    int timer_flag;
    int improv;

private:
    void init();
    HTTPCODE process_read();
    bool process_write(HTTPCODE ret);
    HTTPCODE parse_request_line(char* text);
    HTTPCODE parse_headers(char* text);
    HTTPCODE parse_content(char* text);
    HTTPCODE do_request();
    char* get_line(){return m_read_buffer+m_start_line;}
    LINESTATUS parse_line();
    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int length);
    bool add_linger();
    bool add_blank_line();

private:
    int m_sockfd;
    sockaddr_in m_sockaddr;
    char m_read_buffer[READ_BUFFER_SIZE];
    int m_read_index;
    int m_check_index;
    int m_start_line;
    char m_write_buffer[WRITE_BUFFER_SIZE];
    int m_write_index;
    CHECKSTATE m_check_state;
    METHOD m_method;
    char m_real_file[FIlENAME_LEN];
    char* m_url;
    char* m_version;
    char* m_host;
    int m_content_length;
    bool m_linger;
    char* m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;
    char* m_string;
    int bytes_to_send;
    int byte_have_send;
    char* doc_root;

    locker m_lock;

    map<string, string> m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif