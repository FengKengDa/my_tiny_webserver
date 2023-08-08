#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<list>
#include<pthread.h>
#include<iostream>
#include"../lock/locker.h"
#include"../mysqlpool/sql_connection_pool.h"

using std::list;

template <typename T>
class threadpool
{
private:
    int thread_number;
    int max_request;
    pthread_t *threads;
    list<T*> request_queue;
    locker queue_lock;
    sem queue_sem;
    bool stop;
    connection_pool* conn_pool;

    static void *worker(void *arg);
    void run();

public:
    threadpool(connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T* request, int state);
    bool append_p(T* request);

};

template <typename T>
threadpool<T>::threadpool(connection_pool *connPool, int thread_number, int max_request)
    :conn_pool(connPool), thread_number(thread_number),max_request(max_request)
{
    if(this->max_request<=0||this->thread_number<=0)
    {
        //TODO: log error

        exit(1);
    }
    threads = new pthread_t[max_request];
    if(!threads)
    {
        delete[] threads;
        //TODO: log error

        exit(1);
    }
    for(int i=0;i<thread_number;i++)
    {
        if(pthread_create(threads+i,NULL,worker,this)!=0)
        {
            delete[] threads;
            //TODO: log error

            exit(1);
        }
        if(pthread_detach(threads[i])!=0)
        {
            delete[] threads;
            //TODO: log error

            exit(1);
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] threads;
    stop=true;
}

template <typename T>
bool threadpool<T>::append(T* request, int state)
{
    queue_lock.lock();
    if(request_queue.size()>=max_request)
    {
        //TODO: log error

        return false;
    }
    request->m_state = state;
    request_queue.push_back(request);
    queue_lock.unlock();
    queue_sem.post();
    return true;
}


template <typename T>
bool threadpool<T>::append_p(T *request)
{
    queue_lock.lock();
    if(request_queue.size()>=max_request)
    {
        //TODO: log error

        return false;
    }
    request_queue.push_back(request);
    queue_lock.unlock();
    queue_sem.post();
    return true;
}


template <typename T>
void* threadpool<T>::worker(void *arg)
{
    threadpool* pool = (threadpool*) arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while(!stop)
    {
        queue_sem.wait();
        queue_lock.lock();
        if(request_queue.empty())
        {
            queue_lock.unlock();
            continue;
        }
        T* request = request_queue.front();
        request_queue.pop_front();
        queue_lock.unlock();
        if(!request)
        {
            continue;
        }

        connectionRAII mysql(&request->mysql, conn_pool);
        std::cout<<"enter run"<<std::endl;
        request->process();

    }
}

#endif