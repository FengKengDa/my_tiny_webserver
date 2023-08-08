#ifndef LOCKER_H
#define LOCKER_H

#include<exception>
#include<pthread.h>
#include<semaphore.h>

///这个文件的主要目的是将锁与对象的生命周期结合在一起
///防止忘记解锁等情况

class sem
{
public:
    sem()
    {
        //第二个0表示进程内共享
        sem_init(&m_sem, 0 , 0);
    }
    sem(int num)
    {
        sem_init(&m_sem, 0, num);
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }

    bool wait()
    {
        return sem_wait(&m_sem)==0;
    }

    bool post()
    {
        return sem_post(&m_sem)==0;
    }

private:
    sem_t m_sem;
};

class locker
{
public:
    locker()
    {
        pthread_mutex_init(&m_mutex, NULL);
    }
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex)==0;
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex)==0;
    }
private:
    pthread_mutex_t m_mutex;
};

class cond
{
public:
    cond()
    {
        pthread_cond_init(&m_cond, NULL);
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *m_mutex)
    {
        return pthread_cond_wait(&m_cond, m_mutex)==0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        return pthread_cond_timedwait(&m_cond, m_mutex, &t)==0;
    }

    bool signal()
    {
        return pthread_cond_signal(&m_cond)==0;
    }
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond)==0;
    }

private:
    pthread_cond_t m_cond;
};

#endif