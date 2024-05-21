#include"rpcprovider.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <string>
#include "rpcheader.pb.h"
#include "util.h"
// 框架给外部使用，可以发布rpc方法的函数接口
// 只是简单把 服务描述符 和 方法描述符 全部保存在本地
void RpcProvider::NotifyService(google::protobuf::Service *service){
    ServiceInfo service_info; // ServiceInfo 结构体

    // 获取 service对象的描述信息
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor(); 

    // 获取 service的名字
    std::string service_name = pserviceDesc->name();

    // 获取 服务对象service 方法的数量
    int methodCnt = pserviceDesc->method_count();

    std::cout << "service_name:" << service_name <<std::endl;

    // 获取 service 中每个方法的描述 ， 存入 method_map 中

    for (int i = 0; i < methodCnt; i++)
    {
        const google::protobuf::MethodDescriptor *pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name,pmethodDesc}); // 存入的是方法的名字和描述的地址
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name,service_info}); // 服务名 ，以及服务信息{用于描述服务中的每个 RPC 方法，服务中的<method名，method指针>}

}   

//启动rpc服务节点，开始提供rpc网络调用服务
void RpcProvider::Run(int nodeIndex,short port){

    //获取当前主机的 IP 地址，并将其存储在 ip 变量中。
    char *ipC;
    char hname[128];
    /*
    struct hostent {
    char    *h_name;         official name of host 
    char    **h_aliases;     alias list 
    int     h_addrtype;      host address type 
    int     h_length;        length of address 
    char    **h_addr_list;   list of addresses 
    */
    struct hostent *hent;

    gethostname(hname,sizeof(hname)); //gethostname() 函数用于获取主机的名称，并将其存储在提供的缓冲区中
    hent = gethostbyname(hname); // 根据主机名获取主机的信息，包括它的别名列表和 IP 地址列表。

    for(int i =0;hent->h_addr_list[i];i++){
        ipC = inet_ntoa(*(struct in_addr *)(hent->h_addr_list[i])); // IP地址
    }
    std::string ip = std::string(ipC);

    // 写入文件”test.conf"
    std::string node = "node"+std::to_string(nodeIndex); //node + node id
    std::ofstream outfile;
    outfile.open("test.conf",std::ios::app); // 打开文件并写入

    if(!outfile.is_open()){
        std::cout<<"RpcProvider::Run()--打开文件失败！"<<std::endl;
        exit(EXIT_FAILURE);
    }
    outfile << node + "ip=" + ip <<std::endl;
    outfile << node + "port=" + std::to_string(port) <<std::endl;
    outfile.close();
    //创建服务器
    muduo::net::InetAddress address(ip,port);

    //创建TcpServer对象
    m_muduo_server = std::make_shared<muduo::net::TcpServer>(&m_eventloop,address,"RpcProvider");
    //使用make_shared 直接传入构造参数 ，构造server参数并交给shared_ptr 管理

    //创建完server自然是要绑定回调函数了
    //设置处理新连接的函数
    m_muduo_server->setConnectionCallback(std::bind(&RpcProvider::OnConnection,this,std::placeholders::_1));
    //设置接收到消息时的函数
    m_muduo_server->setMessageCallback(
    std::bind(&RpcProvider::OnMessage,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3)
    );
    // 设置线程的数量
    m_muduo_server->setThreadNum(4);

    //rpc 服务端准备启动，打印信息
    std::cout<<"RpcProvider start service at ip :" <<ip << "port" << port <<std::endl;

    // 启动网络服务
    m_muduo_server->start();

    m_eventloop.loop();
    /*
    Tcpserver 封装 tcpserver的启动，关闭 ，接收客户端数据 ，发送回复的数据。
    eventloop 是事件循环， 一个epollfd对应一个eventloop，在这里监听各种事件，再通过不同的事件类型调用回调函数
    来实现业务的处理。
    网络服务和事件循环时两个相对独立的模块，它们的启动和调用方式都是确定的。
    */

}   

void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn){
    //如果是新连接就啥也不干， 正常接收连接就好了
    if(!conn->connected()){
        //和rpc client的连接断开了
        conn->shutdown();
    }
}

/*
框架内部，RpcProvider 和 RpcConsumer 协商好之间通信的protobuf 数据类型
service_name method_name args(参数)  定义proto的message类型 ，进行数据头的序列化和反序列化
                                     service_name method_name args_size

*/
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr& conn ,muduo::net::Buffer *buffer,
                            muduo::Timestamp)
{
    // 从网络上接收远程rpc调用请求的字符流 Login ARGS
    std::string recv_buf = buffer->retrieveAllAsString();


    // 使用protobuf 的 CodedInputStream 解析数据流
    //1. 将buffer中的数据传给 ArrayInputStream的构造函数，构造一个输入流对象
    google::protobuf::io::ArrayInputStream array_input(recv_buf.data(),recv_buf.size());
    //2. 从ArrayInputStream中读取数据，并且能够识别和解析 protobuf消息的编码格式
    google::protobuf::io::CodedInputStream coded_input(&array_input);

    uint32_t header_size = 0;

    coded_input.ReadVarint32(&header_size);  //解析出一个32位的整数，将结果存储在header_size 中

    //根据header_size读取数据头的原始字符流 ，反序列化数据头，得到rpc请求的详细信息
    std::string rpc_header_str;
    RPC::RpcHeader rpcHeader; //创建数据头对象
    std::string service_name;
    std::string method_name;

    //设置读取 数据头的 限制 ，防止数据读多
    google::protobuf::io::CodedInputStream::Limit msg_limit = coded_input.PushLimit(header_size);
    coded_input.ReadString(&rpc_header_str,header_size); //从coded_input 中读取header_size的数据到rpc_header_str中)
    // 数据头的数据存放在 rpc_header_str中
    //解除读取限制 

    coded_input.PopLimit(msg_limit);

    uint32_t args_size = 0;
    if(rpcHeader.ParseFromString(rpc_header_str))
    {   
        // 数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }else
    {
        // 数据头反序列化失败
        std::cout << "rpc_header_str:" << rpc_header_str <<" parse error" <<std::endl;
        return;
    }


    // 获取rpc方法参数的字符流数据
    std::string args_str; //存放参数的字符流数据
    // 直接读取arg_size长度的字符串数据
    bool read_args_success = coded_input.ReadString(&args_str,args_size);

    if(!read_args_success){
        std::cout<< "RpcProvider::Onmessage 中arg_size 长度的字符串数据读取错误" ;
        return;
    }


    // 在服务Map中寻找需要的服务对象以及服务对象中对应的method对象
    auto it = m_serviceMap.find(service_name);
    if(it == m_serviceMap.end())// 没有找到
    {
        std::cout << "服务：" << service_name << " is not exist!" << std::endl;
        std::cout << "当前已经有的服务列表为:";
        for (auto item : m_serviceMap) 
        {
        std::cout << item.first << " ";
        }
        std::cout << std::endl;
        return;
    }

    auto mit = it->second.m_methodMap.find(method_name);

    if(mit == it->second.m_methodMap.end()){ // 找到service但没找到method
        std::cout << service_name << ":" << method_name << " is not exist!" << std::endl;
        return;
    }

    //service 和 method 都找到了

    google::protobuf::Service *service = it->second.m_service; // 获取需要的service对象
    const google::protobuf::MethodDescriptor *method = mit->second;  // 获取method对象
    // service_info.m_methodMap.insert({method_name,pmethodDesc}); // 存入的是方法的名字和描述的地址

    //生成rpc方法调用的请求request 和 响应 response函数 ，由于是rpc的请求 ，因此请求需要通过request来序列化

    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    //通过调用 service->GetRequestPrototype(method) 方法获取了指定 RPC 方法的 请求消息的原型 （Prototype），
    //然后调用 New() 方法创建了该原型对象的一个新实例，并将其赋值给指向 google::protobuf::Message 类型的指针 request。

    if(!request->ParseFromString(args_str)){ //将args_str中的序列化后的数据 反序列化到 request对象中，
                                             //request对象就有了其中的数据，可以用成员函数来访问
        std::cout<< "request parse error , content:" <<args_str <<std::endl;
        return;
    }

    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // 给下面的method方法的调用，绑定一个Closure的回调函数
    // closure 是执行完本地方法后发生的回调，因此需要序列化和反向发送请求的操作

    google::protobuf::Closure *done =
      google::protobuf::NewCallback<RpcProvider, const muduo::net::TcpConnectionPtr &, google::protobuf::Message *>(
          this, &RpcProvider::SendRpcResponse, conn, response); 

    //调用方法

    /*
    为什么下面这个service->CallMethod 要这么写？或者说为什么这么写就可以直接调用远程业务方法了
    这个service在运行的时候会是注册的service
    // 用户注册的service类 继承 .protoc生成的serviceRpc类 继承 google::protobuf::Service
    // 用户注册的service类里面没有重写CallMethod方法，是 .protoc生成的serviceRpc类 里面重写了google::protobuf::Service中
    的纯虚函数CallMethod，而 .protoc生成的serviceRpc类 会根据传入参数自动调取 生成的xx方法（如Login方法），
    由于xx方法被 用户注册的service类 重写了，因此这个方法运行的时候会调用 用户注册的service类 的xx方法
    真的是妙呀
  */
    service->CallMethod(method,nullptr,request,response,done);
}


// Closure的回调操作，用于序列化rpc的响应和网络发送,发送响应回去
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response) {
  std::string response_str;
  if (response->SerializeToString(&response_str))  // response进行序列化
  {
    // 序列化成功后，通过网络把rpc方法执行的结果发送会rpc的调用方
    conn->send(response_str);
  } else {
    std::cout << "serialize response_str error!" << std::endl;
  }
  
  //    conn->shutdown(); // 模拟http的短链接服务，由rpcprovider主动断开连接  //改为长连接，不主动断开
}

RpcProvider::~RpcProvider() {
  std::cout << "[func - RpcProvider::~RpcProvider()]: ip和port信息：" << m_muduo_server->ipPort() << std::endl;
  m_eventloop.quit();
  //    m_muduo_server.   怎么没有stop函数，奇奇怪怪，看csdn上面的教程也没有要停止，甚至上面那个都没有
}

RpcProvider::RpcProvider()
{

}
