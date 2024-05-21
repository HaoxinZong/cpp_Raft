#include"mutex.h"

SSemaphore::SSemaphore(uint32_t count = 0)
{
    if(sem_init(&m_semaphore,0,count))
    {
        throw std::logic_error("sem_init error");
    }
}

SSemaphore::~SSemaphore()
{
    sem_destroy(&m_semaphore);
}

void SSemaphore::wait()
{
    if(sem_wait(&m_semaphore))
    {
        throw std::logic_error("sem_wait error");
    }
}

void SSemaphore::notify()
{
    if(sem_post(&m_semaphore))
    {
        throw std::logic_error("sem_post error");
    }
}