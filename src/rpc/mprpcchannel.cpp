#include"mprpcchannel.h"
#include<arpa/inet.h> //它定义了一些函数和数据结构，用于处理 IP 地址和网络字节序（Network Byte Order）之间的转换，
#include<netinet/in.h>
#include<sys/socket.h>
#include<unistd.h>
#include<cerrno>
#include"mprpccontroller.h"
#include"rpcheader.pb.h"
#include"util.h" // todo

/*
一条消息由 header_size + service_size method_name args_size + args 用于后面拆分信息
*/

// 所有通过stub代理对象调用的rpc方法，都会走到这里了，
// 统一通过rpcChannel来调用方法
// 统一做rpc方法调用的数据数据序列化和网络发送
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                              google::protobuf::RpcController* controller, const google::protobuf::Message* request,
                              google::protobuf::Message* response, google::protobuf::Closure* done)
{
    if(m_clientFd == -1) // 尚未连接，构造函数中设置的初始值为-1
    {
        std::string errMsg;
        bool rt = newConnect(m_ip.c_str(),m_port, &errMsg);
        if(!rt) {
            //DPrintf("func-MprpcChannel::CallMethod 重连接ip ：{%s} port: {%d} 失败",m_ip.c_str(),m_port);
            controller->SetFailed(errMsg);
            return;
        }else{
            //DPrintf("func-MprpcChannel::CallMethod 连接ip {%s} port{%d} 成功",m_ip.c_str(),m_port);
        }

    }
    
    const google::protobuf::ServiceDescriptor* sd = method->service() ; // 服务描述符 ，从传入的method中来
    std::string servcie_name = sd->name() ;// service_name,服务名称
    std::string method_name = method->name(); //method_name,方法名称

    //获取参数的序列化字符串长度 args_size
    uint32_t args_size{} ; //初始化为0
    std::string args_str;  //序列化request成功后的string存储在这里
    if(request->SerializeToString(&args_str))
    {                                        // 序列化为string成功
        args_size = args_str.size();
    }else
    {
        controller->SetFailed("serialize request error");
        return;
    }
    RPC::RpcHeader rpcHeader; //填充rpcHeader头部
    rpcHeader.set_service_name(servcie_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    std::string rpc_header_str; //序列化为string后的rpc头部存储在这里
    if(!rpcHeader.SerializeToString(&rpc_header_str)){
        controller->SetFailed("Serialize rpc header error!");
        return;
    }

    // 使用protobuf中的CodeOutputStream 来构建最终发送的数据流
    // CodedOutputStream 是 Protocol Buffers 提供的一个类，
    // 用于将数据序列化成二进制形式并写入输出流中。
    // 它的作用是提供了一种高效的方式来序列化数据，尤其适用于大规模数据的序列化。
    std::string send_rpc_str; //存储最终发送的数据
    {
        //创建一个StringOutputStream对象用于写入send_rpc_str 
        google::protobuf::io::StringOutputStream string_output(&send_rpc_str);
        //使用CodedOutputStream方法来构建发送的数据流
        google::protobuf::io::CodedOutputStream coded_output(&string_output);

        //先写入header的长度（变长编码）
        coded_output.WriteVarint32(static_cast<uint32_t>(rpc_header_str.size())); 
        //然后写入rpc_header 本身
        coded_output.WriteString(rpc_header_str);
    }
    send_rpc_str += args_str;  //将请求参数附加到send_rpc_str后面

    // 打印调试信息
  //    std::cout << "============================================" << std::endl;
  //    std::cout << "header_size: " << header_size << std::endl;
  //    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
  //    std::cout << "service_name: " << service_name << std::endl;
  //    std::cout << "method_name: " << method_name << std::endl;
  //    std::cout << "args_str: " << args_str << std::endl;
  //    std::cout << "============================================" << std::endl;
    
    //发送rpc请求
    //失败会再重连一次，若重连失败则直接return
    while(send(m_clientFd,send_rpc_str.c_str(),send_rpc_str.size(),0)==-1){
        char errtxt[512] = {0};
        sprintf(errtxt,"Send Error ! errno: %d" , errno);
        std::cout << "尝试重新连接 ， 对方ip ：" << m_ip << "对方端口：" << m_port << std::endl;
        close(m_clientFd); // 关闭文件描述符 
        m_clientFd = -1;   // 置m_clientFd的值为-1
        std::string errMsg;
        bool rt = newConnect(m_ip.c_str(),m_port,&errMsg);
        if(!rt){                         //若重连未连上，直接返回
            controller->SetFailed(errMsg);
            return;
        }
    }


    /*
    这里将请求发送过去以后 ，rpc服务提供者就会开始处理，返回的时候代表着已经返回响应了
    */

    //接收rpc请求的响应值
    char recv_buf[1024] = {0};
    int recv_size = 0;

    if(-1 == (recv_size = recv(m_clientFd,recv_buf,1024,0))){
        close(m_clientFd); 
        m_clientFd = -1;
        char errtxt[512] = {0};
        sprintf(errtxt,"recv error! errno: %d",errno); //用于将errno中错误的原因写入字符串缓冲区中。
        controller->SetFailed(errtxt);  
        return;
    }

    
    //反序列化rpc调用的响应 response 数据 , reponse 的数据再recv_buf中
    if(!response->ParseFromArray(recv_buf , recv_size)){ // 从recv_buf 缓冲区中读取 recv_size 字节的数据
                                                         // 并将其反序列化到response对象中
        char errtxt[1050] = {0};
        sprintf(errtxt,"parse error! response_str:%s",recv_buf);
        controller->SetFailed(errtxt);
        return;
    }
}


bool MprpcChannel::newConnect(const char* ip,uint16_t port ,string* errMsg){
    int clientfd = socket(AF_INET,SOCK_STREAM,0);
    if(clientfd == -1){ // socket创建失败
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! errno:%d", errno);
        m_clientFd = -1;
        *errMsg = errtxt;
        return false;
    }

    // 存放服务器的ip port信息
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);
    // 连接rpc服务节点
    if (-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "connect fail! errno:%d", errno);
        m_clientFd = -1;
        *errMsg = errtxt; //errMsg中的内容为errtxt
        return false;
     }
    m_clientFd = clientfd;
    return true;
}

MprpcChannel::MprpcChannel(string ip ,short port , bool connectNow):m_ip(ip),m_port(port),m_clientFd(-1)
{
    // 使用TCP ，完成rpc方法的远程调用，使用的是短链接，因此每次都需要重新连上去，待改成长连接
    // rpc 调用方法调用service_name 的method_name服务 ，需要查询zk上该服务器所在的host信息
    // UserServiceRpc /Login
    if(!connectNow)
    {
        return;
    } //可以允许延迟连接
    std::string errMsg;
    auto rt = newConnect(ip.c_str(),port,&errMsg);
    int tryCount = 3;
    while(!rt && tryCount--){
        std::cout<< errMsg <<std::endl;
        std::cout<<" 重连次数还剩余"<< tryCount << "次";
        rt = newConnect(ip.c_str(),port,&errMsg); //最多尝试重连三次
    }
}