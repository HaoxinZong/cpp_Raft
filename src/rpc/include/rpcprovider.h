#pragma once
#include<google/protobuf/descriptor.h>
#include<muduo/net/EventLoop.h>
#include<muduo/net/InetAddress.h>
#include<muduo/net/TcpConnection.h>
#include<muduo/net/TcpServer.h>
#include<functional>
#include<string>
#include<unordered_map>
#include<google/protobuf/service.h>

//发布rpc服务的网络对象类

class RpcProvider{
public:
    // 框架给外部使用，可以发布rpc方法的函数接口
    void NotifyService(google::protobuf::Service *service);

    // 启动rpc服务节点 ，开始提供rpc远程网络调用服务
    void Run(int nodeIndex , short port);

    RpcProvider();
    ~RpcProvider();
private:
    muduo::net::EventLoop m_eventloop;
    std::shared_ptr<muduo::net::TcpServer> m_muduo_server; 

    //service 服务类型信息
    struct ServiceInfo{
        google::protobuf::Service *m_service;  //用于描述服务中的每个 RPC 方法
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> m_methodMap; // 服务中的<方法名，方法>
    };

    //存储注册成功的 service 对象 和 service 中 所有的method信息

    std::unordered_map<std::string,ServiceInfo> m_serviceMap;

    //新的socket连接回调

    void OnConnection(const muduo::net::TcpConnectionPtr & );

    //已连接用户的 读写事件回调函数

    void OnMessage(const muduo::net::TcpConnectionPtr &,muduo::net::Buffer * ,muduo::Timestamp);

    //Closure的回调操作，用于  序列化rpc  的响应 和 网络发送
    
    void SendRpcResponse(const muduo::net::TcpConnectionPtr &,google::protobuf::Message*);



};