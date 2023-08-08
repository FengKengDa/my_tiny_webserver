#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst()
{
    head=NULL;
    tail=NULL;
}

sort_timer_lst::~sort_timer_lst()
{
    util_timer* temp = head;
    while (temp)
    {
        head=temp->nex;
        delete temp;
        temp=head;
    }
}

void sort_timer_lst::add_timer(util_timer* timer)
{
    if(!timer)
    {
        return;
    }
    if(!head)
    {
        head=tail=timer;
        return;
    }
    if(timer->expire<head->expire)
    {
        timer->nex = head;
        head->pre = timer;
        head=timer;
        return;
    }
    add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer* timer)
{
    if(!timer)
    {
        return;
    }
    util_timer *tmp = timer->nex;
    if(!tmp||(timer->expire<tmp->expire))
    {
        return;
    }
    if(timer==head)
    {
        head = head->nex;
        head->pre = NULL;
        timer->nex = NULL;
        add_timer(timer, head);
    }
    else
    {
        timer->pre->nex=timer->nex;
        timer->nex->pre=timer->pre;
        add_timer(timer, timer->nex);
    }
}

void sort_timer_lst::del_timer(util_timer* timer)
{
    if(!timer)
    {
        return;
    }
    if(timer==tail&&timer==head)
    {
        delete timer;
        head=NULL;
        tail=NULL;
        return;
    }
    if(timer==tail)
    {
        tail=tail->pre;
        tail->nex=NULL;
        delete timer;
        return;
    }
    if(timer==head)
    {
        head=head->nex;
        head->pre = NULL;
        delete timer;
        return;
    }
    timer->pre->nex=timer->nex;
    timer->nex->pre=timer->pre;
    delete timer;
}

void sort_timer_lst::tick()
{
    if(!head)
    {
        return;
    }
    time_t cur = time(NULL);
    util_timer* tmp=head;
    while(tmp)
    {
        if(cur<tmp->expire)
        {
            break;
        }
        tmp->cb_func(tmp->user_data);
        head=tmp->nex;
        if(head)
        {
            head->pre = NULL;
        }
        delete tmp;
        tmp=head;
    }
}

void sort_timer_lst::add_timer(util_timer* timer, util_timer* lhead)
{
    util_timer* prev = lhead;
    util_timer* tmp = prev->nex;
    while(tmp)
    {
        if(timer->expire<tmp->expire)
        {
            prev->nex = timer;
            timer->nex = tmp;
            tmp->pre = timer;
            timer->pre = prev;
            break;
        }
        prev=tmp;
        tmp=tmp->nex;
    }
    if(!tmp)
    {
        prev->nex=timer;
        timer->pre=prev;
        timer->nex=NULL;
        tail=timer;
    }
}

void utils::init(int timeslot)
{
    m_timeslot = timeslot;
}

int utils::setnonblocking(int fd)
{
    int old = fcntl(fd, F_GETFL);
    int newo = old | O_NONBLOCK;
    fcntl(fd, F_SETFL, newo);
    return old;
}

void utils::add_fd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void utils::sig_handler(int sig)
{
    int oerrno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = oerrno;
}

void utils::add_sig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL)!=-1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void utils::timer_handler()
{
    timer_lst.tick();
    alarm(m_timeslot);
}

void utils::showerror(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *utils::pipefd = 0;
int utils::u_epollfd = 0;

class utils;
void cb_func(client_data *user_data)
{
    epoll_ctl(utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}
