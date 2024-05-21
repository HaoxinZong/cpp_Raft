#include<thread>
#include<functional>
#include<memory>
#include<pthread.h>
#include<semaphore.h>
#include<stdint.h>
#include<atomic>
#include<list>

#include "noncopyable.h"

//信号量
class SSemaphore : Noncopyable
{   
public: 
    //构造函数 ， count为信号量值的大小
    SSemaphore(uint32_t count = 0);
    //析构函数
    ~SSemaphore();

    // 获取信号量
    void wait() ;

    // 释放信号量

    void notify();
private:
    sem_t m_semaphore;
};

// 局部锁的模板实现
template<class T>
struct ScopedLockImpl
{
public:
//构造函数
ScopedLockImpl(T &mutex):m_mutex(mutex)
{
    m_mutex.lock();
    m_locked = true;
}
~ScopedLockImpl() {
    unlock();
}

void lock()
{
    if(!m_locked)
    {
        m_mutex.lock();
        m_locked = true;
    }
}

void unlock()
{
    if(m_locked)
    {
        m_mutex.unlock();
        m_locked = false;
    }
}
private:
    /// mutex
    T& m_mutex;
    /// 是否已上锁
    bool m_locked;
};

//局部读锁的实现
template<class T>
struct ReadScopedLockImpl {
public:
    /**
     * @brief 构造函数
     * @param[in] mutex 读写锁
     */
    ReadScopedLockImpl(T& mutex)
        :m_mutex(mutex) {
        m_mutex.rdlock();
        m_locked = true;
    }

    /**
     * @brief 析构函数,自动释放锁
     */
    ~ReadScopedLockImpl() {
        unlock();
    }

    /**
     * @brief 上读锁
     */
    void lock() {
        if(!m_locked) {
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    /**
     * @brief 释放锁
     */
    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    /// mutex
    T& m_mutex;
    /// 是否已上锁
    bool m_locked;
};


/**
 * @brief 局部写锁模板实现
 */
template<class T>
struct WriteScopedLockImpl {
public:
    /**
     * @brief 构造函数
     * @param[in] mutex 读写锁
     */
    WriteScopedLockImpl(T& mutex)
        :m_mutex(mutex) {
        m_mutex.wrlock();
        m_locked = true;
    }

    /**
     * @brief 析构函数
     */
    ~WriteScopedLockImpl() {
        unlock();
    }

    /**
     * @brief 上写锁
     */
    void lock() {
        if(!m_locked) {
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    /**
     * @brief 解锁
     */
    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    /// Mutex
    T& m_mutex;
    /// 是否已上锁
    bool m_locked;
};

// 互斥量 ， 用pthread_mutex 实现
class Mutex : Noncopyable
{   ///局部锁
   
public: 
    typedef ScopedLockImpl<Mutex> Lock;

    // 构造函数
    Mutex()
    {
        pthread_mutex_init(&m_mutex ,nullptr);
    }

    ~Mutex()
    {
        pthread_mutex_destroy(&m_mutex);
    }

    void lock()
    {
        pthread_mutex_lock(&m_mutex);
    }

    void unlock()
    {
        pthread_mutex_unlock(&m_mutex);
    }
private:
    /// mutex
    pthread_mutex_t m_mutex;
};

//读写互斥量

class RWMutex : Noncopyable
{
public:
    // 局部读锁
    typedef ReadScopedLockImpl<RWMutex> ReadLock;

    // 局部写锁
    typedef WriteScopedLockImpl<RWMutex> WriteLock;

    // 构造函数

    RWMutex()
    {
        pthread_rwlock_init(&m_lock,nullptr);
    } 

    ~RWMutex()
    {
        pthread_rwlock_destroy(&m_lock);
    }

    /**
     * @brief 上读锁
     */
    void rdlock() {
        pthread_rwlock_rdlock(&m_lock);
    }

    /**
     * @brief 上写锁
     */
    void wrlock() {
        pthread_rwlock_wrlock(&m_lock);
    }

    /**
     * @brief 解锁
     */
    void unlock() {
        pthread_rwlock_unlock(&m_lock);
    }
private:
    /// 读写锁
    pthread_rwlock_t m_lock;
};

//自旋锁
class Spinlock : Noncopyable
{
public:
    // 局部锁
    typedef ScopedLockImpl<Spinlock> Lock;

    /**
     * @brief 构造函数
     */
    Spinlock() {
        pthread_spin_init(&m_mutex, 0);
    }

    /**
     * @brief 析构函数
     */
    ~Spinlock() {
        pthread_spin_destroy(&m_mutex);
    }

    /**
     * @brief 上锁
     */
    void lock() {
        pthread_spin_lock(&m_mutex);
    }

    /**
     * @brief 解锁
     */
    void unlock() {
        pthread_spin_unlock(&m_mutex);
    }
private:
    /// 自旋锁
    pthread_spinlock_t m_mutex;
};


//原子锁
class CASLock : Noncopyable {
public:
    /// 局部锁
    typedef ScopedLockImpl<CASLock> Lock;

    /**
     * @brief 构造函数
     */
    CASLock() {
        m_mutex.clear();
    }

    /**
     * @brief 析构函数
     */
    ~CASLock() {
    }

    /**
     * @brief 上锁
     */
    void lock() {
        while(std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire));
    }

    /**
     * @brief 解锁
     */
    void unlock() {
        std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
    }
private:
    /// 原子状态
    volatile std::atomic_flag m_mutex;
};