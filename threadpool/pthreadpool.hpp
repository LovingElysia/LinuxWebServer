//
// Created by elysia on 24-3-11.
//
#ifndef PTHREADPOOL_H
#define PTHREADPOOL_H
#include "../lock/locker.h"
#include <list>
#include <cstdio>
#include "../CGImysql/connection_pool.h"
#include <unistd.h>
template <typename T>
class pthreadpool
{
public:
    pthreadpool(const int&actor_model,connection_pool* connpool,const int& thread_number,const int& requests_number);
    ~pthreadpool();
    bool append(T* request,const int& stat);
    bool append_p(T* request);
private:
    static void* worker(void* arg);
    void run();
private:
    int m_max_thread_number;
    int m_max_requests_number;
    pthread_t* m_threads;
    std::list<T*> m_worker_queue;
    locker m_queuelocker;
    sem m_queuestat;
    connection_pool* m_connpool;
    int m_actor_model;
};
template <typename T>
pthreadpool<T>::pthreadpool(const int&actor_model,connection_pool* connpool,
                            const int& thread_number,const int& requests_number):m_actor_model{actor_model},m_connpool{connpool},m_max_thread_number{thread_number},m_max_requests_number{requests_number}
{
    //判断创建线程数和多任务数是否合法
    if((m_max_thread_number<=0)||(m_max_requests_number<=0))
    {
        throw std::exception();
    }
    //为线程池分配内存
    m_threads = new pthread_t[m_max_thread_number];
    if(!m_threads)
    {
        throw std::exception();
    }
    //创建并分离线程
    for(int i=0;i<m_max_thread_number;++i)
    {
        if(pthread_create(&m_threads[i],nullptr,worker,this)!=0)
        {
            delete []m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i]))
        {
            delete []m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
pthreadpool<T>::~pthreadpool()
{
    delete []m_threads;
}

template <typename T>
bool pthreadpool<T>::append(T* request,const int& stat)
{
    //设置临界区
    m_queuelocker.lock();
    //检查任务队列的未处理任务数量是否超出限制
    if(m_worker_queue.size()>= m_max_requests_number)
    {
        m_queuelocker.unlock();
        return false;
    }
    //未超出限制加入队列
    request->m_stat = stat;
    m_worker_queue.push_back(request);
    //释放临界区
    m_queuelocker.unlock();
    //告诉工作线程有任务到来
    m_queuestat.post();
    return true;
}

template <typename T>
bool pthreadpool<T>::append_p(T* request)
{
    //设置临界区
    m_queuelocker.lock();
    //检查任务队列的未处理任务数量是否超出限制
    if(m_worker_queue.size()>= m_max_requests_number)
    {
        m_queuelocker.unlock();
        return false;
    }
    //未超出限制加入队列
    m_worker_queue.push_back(request);
    //释放临界区
    m_queuelocker.unlock();
    //告诉工作线程有任务到来
    m_queuestat.post();
    return true;
}

template <typename T>
void* pthreadpool<T>::worker(void* arg)
{
    pthreadpool* pool = (pthreadpool*)(arg);
    pool->run();
    return pool;
}

template <typename T>
void pthreadpool<T>::run()
{
    while (true)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_worker_queue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_worker_queue.front();
        m_worker_queue.pop_front();
        m_queuelocker.unlock();
        if (!request)
            continue;
        if (1 == m_actor_model)
        {
            if (0 == request->m_stat)
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connpool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            connectionRAII mysqlcon(&request->mysql, m_connpool);
            request->process();
        }
    }
}
#endif
