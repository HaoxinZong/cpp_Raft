
#include <arpa/inet.h>
#include <netinet/in.h>

#include "clerk.h"

#include "raftServerRpcUtil.h"
#include <string>
#include <vector>
std::string Clerk::Get(std::string key) {
  m_requestId++;
  auto requestId = m_requestId;
  int server = m_recentLeaderId;
  raftKVRpcProctoc::GetArgs args;
  args.set_key(key);
  args.set_clientid(m_clientId);
  args.set_requestid(requestId);

  while (true) {
    raftKVRpcProctoc::GetReply reply;
    bool ok = m_servers[server]->Get(&args, &reply);
    if (!ok ||
        reply.err() ==
            ErrWrongLeader) {  //会一直重试，因为requestId没有改变，因此可能会因为RPC的丢失或者其他情况导致重试，kvserver层来保证不重复执行（线性一致性）
      server = (server + 1) % m_servers.size();
      continue;
    }
    if (reply.err() == ErrNoKey) {
      return "";
    }
    if (reply.err() == OK) {
      m_recentLeaderId = server;
      return reply.value();
    }
  }
  return "";
}

void Clerk::PutAppend(std::string key, std::string value, std::string op) {
  // You will have to modify this function.
  m_requestId++;
  auto requestId = m_requestId;
  auto server = m_recentLeaderId;
  while (true) {
    raftKVRpcProctoc::PutAppendArgs args;
    args.set_key(key);
    args.set_value(value);
    args.set_op(op);
    args.set_clientid(m_clientId);
    args.set_requestid(requestId);
    raftKVRpcProctoc::PutAppendReply reply;
    bool ok = m_servers[server]->PutAppend(&args, &reply);
    if (!ok || reply.err() == ErrWrongLeader) {
        //DPrintf("[Clerk::PutAppend]向尝试连接的leader:{%d}请求失败，向新leader{%d}重试  ，操作：{%s}", server, server + 1,
              //op.c_str());
        printf("[Clerk::PutAppend]向尝试连接的leader:{%d}请求失败，向新leader{%d}重试  ，操作：{%s}", server, server + 1,
              op.c_str());
              std::cout<<endl;
      if (!ok) {
        //DPrintf("重试原因 ,rpc失败 ");
        printf("重试原因 ,rpc失败 ");
        std::cout<<endl;
      }
      if (reply.err() == ErrWrongLeader) {
        printf("重试原因:非leader");
        std::cout<<endl;
      }
      server = (server + 1) % m_servers.size();  // try the next server
      continue;
    }
    if (reply.err() == OK) {  //在 Raft 中，当 Leader 收到客户端的请求并且成功将该请求在 Raft 日志中复制并提交后，就会向客户端发送成功的响应，此时 reply.err() 就会返回 OK。
      m_recentLeaderId = server;
      return;
    }
  }
}

void Clerk::Put(std::string key, std::string value) { PutAppend(key, value, "Put"); }

void Clerk::Append(std::string key, std::string value) { PutAppend(key, value, "Append"); }
//初始化客户端
void Clerk::Init(std::string configFileName) {
  //获取所有raft节点ip、port ，并进行连接
  MprpcConfig config;
  config.LoadConfigFile(configFileName.c_str());
  std::vector<std::pair<std::string, short>> ipPortVt;
  for (int i = 0; i < INT_MAX - 1; ++i) {
    std::string node = "node" + std::to_string(i);

    std::string nodeIp = config.Load(node + "ip");
    std::string nodePortStr = config.Load(node + "port");
    if (nodeIp.empty()) {
      break;
    }
    ipPortVt.emplace_back(nodeIp, atoi(nodePortStr.c_str()));  //沒有atos方法，可以考慮自己实现
  }
  //进行连接
  for (const auto& item : ipPortVt) {
    std::string ip = item.first;
    short port = item.second;
    // 2024-01-04 todo：bug fix
    auto* rpc = new raftServerRpcUtil(ip, port);
    m_servers.push_back(std::shared_ptr<raftServerRpcUtil>(rpc));
  }
}

Clerk::Clerk() : m_clientId(Uuid()), m_requestId(0), m_recentLeaderId(0) {}