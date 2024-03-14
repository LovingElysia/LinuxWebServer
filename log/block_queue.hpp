//
// Created by elysia on 24-3-11.
//
#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H
#include "../lock/locker.h"
#include <stdlib.h>

template <class T>
class block_queue
{
public:
    block_queue(int max_size);
    ~block_queue();
public:
    void clear();
    bool full(); //判断队列是否满
    bool front(T& value);
    bool back(T& value);
    int size();
    bool push_back(const T& item);
    bool pop_front(T& item);
private:
    locker m_mutex;
    cond m_cond;

    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;
};

template <class T>
block_queue<T>::block_queue(int max_size)
{
    if (max_size <= 0)
        exit(-1);
    m_array = new T[max_size];
    m_max_size = max_size;
    m_size = 0;
    m_front = -1;
    m_back = -1;
}

template <class T>
block_queue<T>::~block_queue()
{
    m_mutex.lock();
    if(m_array!=nullptr)delete []m_array;
    m_mutex.unlock();
}

template <class T>
void block_queue<T>::clear()
{
    m_mutex.lock();
    m_size = 0;
    m_front = -1;
    m_back = -1;
    m_mutex.unlock();
}

template <class T>
bool block_queue<T>::full()
{
    m_mutex.lock();
    if(m_size >= m_max_size)
    {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template <class T>
bool block_queue<T>::front(T& value)
{
    m_mutex.lock();
    if(0 == m_size)
    {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_front];
    m_mutex.unlock();
    return true;
}

template <class T>
bool block_queue<T>::back(T& value)
{
    m_mutex.lock();
    if(0 == m_size)
    {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_back];
    m_mutex.unlock();
    return true;
}

template <class T>
int block_queue<T>::size()
{
    m_mutex.lock();
    const int tmp{m_size};
    m_mutex.unlock();
    return tmp;
}

template <class T>
bool block_queue<T>::push_back(const T& item)
{
    m_mutex.lock();
    if(m_size >= m_max_size)
    {
        m_cond.broadcast();
        m_mutex.unlock();
        return false;
    }
    m_back = (m_back+1)%m_max_size;
    m_array[m_back] = item;

    m_size++;

    m_cond.broadcast();
    m_mutex.unlock();
    return true;
}

template <class T>
bool block_queue<T>::pop_front(T& item)
{
    m_mutex.lock();
    while (m_size <= 0)
    {
        if(!m_cond.wait(m_mutex.get()))
        {
            m_mutex.unlock();
            return false;
        }
    }
    m_front = (m_front+1)%m_max_size;
    item = m_array[m_front];

    m_size --;

    m_mutex.unlock();
    return true;
}
#endif