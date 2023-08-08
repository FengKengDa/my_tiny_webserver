#ifndef LST_TIMER
#define LST_TIMER

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
#include <time.h>

class util_timer;

struct client_data
{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

class util_timer
{
public:
    util_timer():pre(NULL),nex(NULL){}

    time_t expire;
    void (* cb_func)(client_data *);
    client_data* user_data;
    util_timer* pre;
    util_timer* nex;
};

class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer*);
    void adjust_timer(util_timer*);
    void del_timer(util_timer*);
    void tick();

private:
    void add_timer(util_timer*, util_timer*);

    util_timer* head;
    util_timer* tail;
};

class utils
{
public:
    utils(){};
    ~utils(){};

    void init(int);

    int setnonblocking(int);

    void add_fd(int epollfd, int fd, bool oneshot, int trigmode);

    static void sig_handler(int sig);

    void add_sig(int sig, void(handler)(int), bool restart=true);

    void timer_handler();

    void showerror(int fd, const char *info);
public:
    static int* pipefd;
    sort_timer_lst timer_lst;
    static int u_epollfd;
    int m_timeslot;
};

void cb_func(client_data* user_data);




#endif