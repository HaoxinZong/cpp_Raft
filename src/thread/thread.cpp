#include"thread.h"
#include"utils.h"

static thread_local SThread *t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKONWN";

// 获取当前线程的指针
SThread* SThread::GetThis()
{
    return t_thread;
}

const std::string &SThread::GetName()
{
    return t_thread_name;
}


void SThread::SetName(const std::string &name)
{
    if(name.empty())
    {
        return;
    }
    if(t_thread)
    {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

SThread::SThread(std::function<void()> cb ,const std::string &name=""): m_cb(cb), m_name(name)
{
    int rt = pthread_create(&m_thread,nullptr,&SThread::run,this); //创建成功返回 0 
    if(rt){
        throw std::logic_error("pthread_create error");
    }
    m_semaphore.wait();
}

SThread::~SThread(){
    if(m_thread){
        pthread_detach(m_thread); //等待线程完成任务后自动退出
    }
}
void *SThread::run(void *args)
{
    SThread *thread = (SThread*) args;
    t_thread =thread;
    t_thread_name =thread->m_name;
    thread->m_id = GetThreadId();
    pthread_setname_np(pthread_self(),thread->m_name.substr(0,15).c_str());

    std::function<void()> cb;
    cb.swap(thread->m_cb);

    thread->m_semaphore.notify();

    cb();
    return 0;
}

void SThread::join()
{
    if(m_thread)
    {
        int rt = pthread_join(m_thread,nullptr);
        if(rt){
            throw std::logic_error("pthread_join error");
        }
        m_thread = 0;
    }
}