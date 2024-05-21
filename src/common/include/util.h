#pragma once
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/access.hpp>
#include <condition_variable>  // pthread_condition_t
#include <functional>
#include <iostream>
#include <mutex>  // pthread_mutex_t
#include <queue>
#include <random>
#include <sstream>
#include <thread>
#include "config.h"



class Defer final
{
public:
    explicit Defer(std::function<void()> fun) : m_funCall(fun) {}
    ~Defer() { m_funCall(); }

private:
    std::function<void()> m_funCall;
};


template<class F>
class DeferClass{
public:
    DeferClass(F&& f): m_func(std::forward<F>(f)){}
    DeferClass(const F &f) :m_func(f) {}
    ~DeferClass() {m_func();} // 在析构函数中执行函数，达到延迟执行的目的

    DeferClass(const DeferClass& e) = delete; //禁用拷贝构造函数。
    DeferClass& operator= (const DeferClass& e) = delete; //赋值运算符，确保不会意外复制延迟执行对象。


private:
F m_func;

};

#define _CONCAT(a,b) a##b   //这个宏定义了一个简单的连接操作符，用于将两个标识符连接成一个单独的标识符。
#define _MAKE_DEFER_(line) DeferClass _CONCAT(defer_placeholder,line) = [&]()
// _MAKE_DEFER_(line) 它使用了上面定义的连接操作符 _CONCAT，将 defer_placeholder 和 line 连接在一起，从而创建了一个唯一的标识符。


#undef DEFER
#define DEFER _MAKE_DEFER(__LINE__)

void DPrintf(const char* format, ...);

void myAssert(bool condition,std::string message = "Assertion failed!");



template <typename ...Args>
std::string format(const char* format_str, Args ...args){
    std::stringstream ss;
    int _[] = {((ss << args),0)...};
    (void)_;
    return ss.str();
}
/*
template <typename... Args>：
这是一个模板函数声明，使用了可变参数模板（Variadic Templates）的特性，表示该函数可以接受任意数量的参数。
std::string format(const char* format_str, Args... args)：
这是函数的声明，它接受两个参数：format_str 表示格式串，args... 表示可变数量的参数。
std::stringstream ss;：
创建了一个 std::stringstream 对象 ss，用于构建格式化后的字符串。
int _[] = {((ss << args), 0)...};：
这是一个折叠表达式（Fold Expression），用于对可变参数展开并执行操作。
对于每个参数 args，表达式 (ss << args) 会将参数写入到 stringstream 对象 ss 中，并逗号运算符的结果是将其丢弃，最终得到的是一个整型数组。
这个整型数组用于初始化一个无名的整型数组 _[]，数组的大小与可变参数的数量相同，但实际上数组中的元素都是 0。
(void)_;：
这里使用了 (void) 将整型数组 _[] 强制转换为 void 类型，目的是抑制编译器对未使用变量的警告。
return ss.str();：
最后，返回 stringstream 对象 ss 中存储的格式化后的字符串，通过 str() 方法将其转换为 std::string 类型并返回。
*/


//用于获取当前时间点。它返回的是一个时钟对象（high_resolution_clock）的当前时间点，即当前系统时间。
std::chrono::_V2::system_clock::time_point now();



std::chrono::milliseconds getRandomizedElectionTimeout(); //随机化选举事件
void sleepNMilliseconds(int N);
////////////////////////////获取可用端口
/// @brief 判断当前port是否释放
/// @param usPort 
/// @return true false
bool isReleasePort(unsigned short usPort);

bool getReleasePort(short& port);


// ////////////////////////异步写日志的日志队列
template <typename T>
class LockQueue {
 public:
  // 多个worker线程都会写日志queue
  void Push(const T& data) {
    std::lock_guard<std::mutex> lock(m_mutex);  //使用lock_gurad，即RAII的思想保证锁正确释放
    m_queue.push(data);
    m_condvariable.notify_one();
  }

  // 一个线程读日志queue，写日志文件
  T Pop() {
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_queue.empty()) {
      // 日志队列为空，线程进入wait状态
      m_condvariable.wait(lock);  //这里用unique_lock是因为lock_guard不支持解锁，而unique_lock支持
    }
    T data = m_queue.front();
    m_queue.pop();
    return data;
  }

  bool timeOutPop(int timeout, T* ResData)  // 添加一个超时时间参数，默认为 50 毫秒
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    // 获取当前时间点，并计算出超时时刻
    auto now = std::chrono::system_clock::now();
    auto timeout_time = now + std::chrono::milliseconds(timeout);

    // 在超时之前，不断检查队列是否为空
    while (m_queue.empty()) {
      // 如果已经超时了，就返回一个空对象
      if (m_condvariable.wait_until(lock, timeout_time) == std::cv_status::timeout) {
        return false;
      } 
    }

    T data = m_queue.front();
    m_queue.pop();
    *ResData = data;
    return true;
  }

 private:
  std::queue<T> m_queue;
  std::mutex m_mutex;
  std::condition_variable m_condvariable;
};

// Op 是 kvserver 传递给 raft 的command
class Op{
public:
  // 
  std::string Operation; // "Get" "Put" "Append"
  std::string Key;
  std::string Value;
  std::string ClientId;  // 客户端的id
  int RequestId;         // 客户端号码请求的Request的序列号，为了保证线性一致性


  //////////////////////func here
  std::string asString() const
  {
    std::stringstream ss;
    boost::archive::text_oarchive oa(ss);

    // 向档案中写入实例
    oa<< *this;

    return ss.str();
  }

  bool parseFromString(std::string str) {
        std::stringstream iss(str);
        boost::archive::text_iarchive ia(iss);
        // read class state from archive
        ia >> *this;
        return true; //todo : 解析失敗如何處理，要看一下boost庫了
    }
  
   friend std::ostream& operator<<(std::ostream& os, const Op& obj) {
        os << "[MyClass:Operation{"+obj.Operation+"},Key{"+obj.Key+"},Value{"+obj.Value +"},ClientId{"+obj.ClientId+"},RequestId{"+std::to_string( obj.RequestId)+"}"; // 在这里实现自定义的输出格式
        return os;
    }

private:
friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & Operation;
        ar & Key;
        ar & Value;
        ar & ClientId;
        ar & RequestId;
    }
};

const std::string OK = "OK";
const std::string ErrNoKey = "ErrNoKey";
const std::string ErrWrongLeader = "ErrWrongLeader";

