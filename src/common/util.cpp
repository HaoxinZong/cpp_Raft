#include"util.h"
#include<chrono>
#include<cstdarg>
#include<cstdio>
#include<ctime>
#include<iomanip>

void myAssert(bool condition, std::string message) { // assertion 不正确时输出错误并退出
  if (!condition) {
    std::cerr << "Error: " << message << std::endl;
    std::exit(EXIT_FAILURE);
  }
}


//用于获取当前时间点。它返回的是一个时钟对象（high_resolution_clock）的当前时间点，即当前系统时间。
std::chrono::_V2::system_clock::time_point now() 
{ 
    return std::chrono::high_resolution_clock::now(); 
}

//均匀分布的整数随机数生成器
std::chrono::milliseconds getRandomizedElectionTimeout() {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> dist(minRandomizedElectionTime, maxRandomizedElectionTime);//这个时间在config.h中设置

  return std::chrono::milliseconds(dist(rng));
}

void sleepNMilliseconds(int N) { 
    std::this_thread::sleep_for(std::chrono::milliseconds(N)); 
    }

bool getReleasePort(short &port) {
  short num = 0;
  while (!isReleasePort(port) && num < 30) {
    ++port;
    ++num;
  }
  if (num >= 30) {
    port = -1;
    return false;
  }
  return true;
}

bool isReleasePort(unsigned short usPort) {
  int s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(usPort);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  int ret = ::bind(s, (sockaddr *)&addr, sizeof(addr));
  if (ret != 0) {
    close(s);
    return false;
  }
  close(s);
  return true;
}

void DPrintf(const char *format, ...) {
  if (Debug) {                             // config.中定义
    // 获取当前的日期，然后取日志信息，写入相应的日志文件当中 a+
    time_t now = time(0);
    tm *nowtm = localtime(&now);
    va_list args;
    va_start(args, format);
    std::printf("[%d-%d-%d-%d-%d-%d] ", nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mday, nowtm->tm_hour,
                nowtm->tm_min, nowtm->tm_sec);
    std::vprintf(format, args);
    std::printf("\n");
    va_end(args);
  }
}



