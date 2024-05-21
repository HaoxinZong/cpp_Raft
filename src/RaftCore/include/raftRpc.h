#pragma once 
#include"raftRPC.pb.h"

///@brief 维护当前节点对其他某一个结点的所有rpc发送通信的功能
class RaftRpc
{
private: 
    raftRpcProctoc::raftRpc_Stub *stub_ ;
public:
    //主动调用其他节点的三个方法
    bool AppendEntries(raftRpcProctoc::AppendEntriesArgs* args,raftRpcProctoc::AppendEntriesReply* response);

    bool InstallSnapshot(raftRpcProctoc::InstallSnapshotRequest* args,raftRpcProctoc::InstallSnapshotResponse *response);

    bool RequestVote(raftRpcProctoc::RequestVoteArgs *args ,raftRpcProctoc::RequestVoteReply *response);
    /// @brief 
    /// @param ip 远端ip 
    /// @param port 远端端口
    RaftRpc(std::string ip ,short port);
    ~RaftRpc();
};