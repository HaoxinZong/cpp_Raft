#include "mutex.h"

class SThread : Noncopyable
{
public:
    // 线程只能指针类型
    typedef std::shared_ptr<SThread> T_ptr;

    /// @brief 构造函数
    /// @param cb  线程执行的函数
    /// @param name  线程名称
    SThread(std::function<void()> cb ,const std::string &name = "");
    /// @brief 析构函数
    ~SThread();

    // 获取线程id
    pid_t getId() const{return m_id;}

    // 获取线程的名称
    const std::string &getName() const {return m_name;}

    // 等待线程执行完成
    void join();

    // 获取当前线程的指针
    static SThread *GetThis();

    // 获取当前线程的名称
    static const std::string &GetName();

    // 设置当前线程的名称
    static void SetName(const std::string &name);

private:
    //线程id 
    pid_t m_id = -1;
    //线程结构
    pthread_t m_thread = 0; //将 m_thread 初始化为 0 可以用于标识该变量目前并未指向任何有效的线程。在实际使用中，
                            //通常会在创建线程之后，将 pthread_create 函数返回的线程标识符赋值给 m_thread 变量，
                            //从而使其指向新创建的线程。
    //线程执行函数
    std::function<void()> m_cb;
    //线程名称
    std::string m_name;
    //信号量
    SSemaphore m_semaphore;

    //线程执行函数
    static void*run(void *args);
};