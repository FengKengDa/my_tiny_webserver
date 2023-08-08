#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"
#include "./timer/lst_timer.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define TIMESLOT 5

class webserver
{
public:
    webserver();
    ~webserver();

    void init(int port, string user, string password, string database_name, 
        int log_write, int opt_linger, int trigmode, int sql_num, int thread_num,
        int close_log, int actor_model);
    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void event_listen();
    void event_loop();
    void timer(int connfd, struct sockaddr_in client_addr);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);
    bool dealclientdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

    int m_port;
    char* m_root; //文件存放路径
    int m_log_write;
    int m_close_log;
    int m_actormode;

    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    connection_pool *m_connpool;
    string m_user;
    string m_password;
    string m_database_name;
    int m_mysql_num;

    threadpool<http_conn> *m_pool;
    int m_thread_num;

    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_opt_linger;
    int m_trigmode;
    int m_lisnten_trigmode;
    int m_conn_trigmode;

    client_data* users_timer;
    utils m_utils;
};
#endif