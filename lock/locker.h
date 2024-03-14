//
// Created by elysia on 24-3-11.
//

#ifndef LOCKER_H
#define LOCKER_H
#include <exception>
#include <semaphore.h>
#include <pthread.h>

class sem
{
public:
    sem();
    sem(const int &value);
    ~sem();

public:
    bool wait();
    bool post();

private:
    sem_t m_sem;
};

class locker
{
public:
    locker();
    ~locker();

public:
    bool lock();
    bool unlock();
    pthread_mutex_t *get();

private:
    pthread_mutex_t m_mutex;
};

class cond
{
public:
    cond();
    ~cond();

public:
    bool wait(pthread_mutex_t *mutex);
    bool wait(pthread_mutex_t *mutex, const struct timespec *t);
    bool signal();
    bool broadcast();

private:
    pthread_cond_t m_cond;
};
#endif
