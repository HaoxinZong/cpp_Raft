#include<google/protobuf/descriptor.h>
#include<google/protobuf/message.h>
#include<google/protobuf/service.h>
#include<algorithm>
#include<functional>
#include<iostream>
#include<map>
#include<random>
#include<string>
#include<unordered_map>
#include<vector>


using namespace std;

class MprpcChannel : public google::protobuf::RpcChannel {
private:
int m_clientFd; // 这个channel连接的客户端的fd
const std::string m_ip;
const uint16_t m_port;

///@brief 连接ip和端口 ，并设置m_clientFd
///@param ip  ip地址 ，本机字节序
///@param port port端口 本机字节序
///@return 成功返回空字符串，否则返回失败信息
bool newConnect(const char *ip ,uint16_t port ,string *errMsg);
public:
MprpcChannel(string ip,short port ,bool connectNow);
void CallMethod(const google::protobuf::MethodDescriptor *method , 
                //方法描述符，用于表示一个 RPC 服务中的方法。它包含了方法的名称、输入消息类型、输出消息类型等信息。
                google::protobuf::RpcController *controller,
                //RPC 控制器，用于控制 RPC 调用的行为。它是一个抽象类，提供了控制 RPC 调用流程的方法，例如取消调用、获取错误信息等。
                const google::protobuf::Message *request, 
                //消息类型，用于表示 RPC 方法的输入和输出参数。它是一个抽象类，可以通过派生来定义具体的消息类型。
                google::protobuf::Message * response,
                //如上
                google::protobuf::Closure *done) override;

};