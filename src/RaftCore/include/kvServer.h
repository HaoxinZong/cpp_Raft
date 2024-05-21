#pragma once
#include <boost/any.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/foreach.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include "kvServerRPC.pb.h"
#include "raft.h"
class KvServer :raftKVRpcProctoc::kvServerRpc
{
private:
    std::mutex m_mtx;
    int m_me;
    std::shared_ptr<Raft> m_raftNode;
    std::shared_ptr<LockQueue<ApplyMsg>> applyChan; //kvserver 和 raft节点通信的管道

    int m_maxRaftState; //当log增长到这个数量，执行snapshot
    
    //SkipList<std::string, std::string> m_skipList;
    
    std::string m_serializedKVData; //序列化后的kv数据

    std::unordered_map<std::string,std::string> m_kvDB ; //map构建的kvDB,和上层状态机沟通的队列

    std::unordered_map<int,LockQueue<Op> *> waitApplyCh; // waitApplyCh 是一个map ，键是int
                                                         // 值是Op类型的管道 Op 是 kvserver 传递给 raft 的command
    //
    std::unordered_map<std::string,int> m_lastRequestId;  // clientid 对应的客户端发出过的requestID clientid -> requestID  //一个kV服务器可能连接多个client

    //上一次snapshot的Index , 是一个raft index
    int m_lastSnapShotRaftLogIndex;
public:
    KvServer() = delete;
    KvServer(int me, int maxraftstate,std::string nodeInfoFileName, short port); //唯一的构造函数

    //将 GetArgs 改为rpc调用的，因为是远程客户端，即服务器宕机对客户端来说是无感的
    void Get(const raftKVRpcProctoc::GetArgs *args,raftKVRpcProctoc::GetReply*reply);

     // clerk 使用RPC远程调用
    void PutAppend(const raftKVRpcProctoc::PutAppendArgs *args, raftKVRpcProctoc::PutAppendReply *reply);
    // 判断当前的request是不是重复的request
    bool ifRequestDuplicate(std::string ClientId, int RequestId);

    void ReadSnapShotToInstall(std::string snapshot);

    ////一直等待raft传来的applyCh
    void ReadRaftApplyCommandLoop();
    
    // Handler the Command from kv.rf.applyCh
    void GetCommandFromRaft(ApplyMsg message);

    // Handler the SnapShot from kv.rf.applyCh
    void GetSnapShotFromRaft(ApplyMsg message);

    // 检查是否需要制作快照，需要的话就向raft之下制作快照
    void IfNeedToSendSnapShotCommand(int raftIndex, int proportion);

    // 制作snapshot
    std::string MakeSnapShot();

    
    bool SendMessageToWaitChan(const Op &op, int raftIndex);
    
    // 打印出kvdb的数据
    void DprintfKVDB();
    // 对KvDB执行op对应的append操作
    void ExecuteAppendOpOnKVDB(Op op);
    // 对kvDB执行op对应的get操作
    void ExecuteGetOpOnKVDB(Op op, std::string *value, bool *exist);
    // 对kvDB执行op对应的put操作
    void ExecutePutOpOnKVDB(Op op);
public:  // for rpc
    void PutAppend(google::protobuf::RpcController *controller, const ::raftKVRpcProctoc::PutAppendArgs *request,
                 ::raftKVRpcProctoc::PutAppendReply *response, ::google::protobuf::Closure *done) override;

    void Get(google::protobuf::RpcController *controller, const ::raftKVRpcProctoc::GetArgs *request,
           ::raftKVRpcProctoc::GetReply *response, ::google::protobuf::Closure *done) override;


        /////////////////serialiazation start ///////////////////////////////
    //notice ： func serialize
private:
    friend class boost::serialization::access;

    // When the class Archive corresponds to an output archive, the
    // & operator is defined similar to <<.  Likewise, when the class Archive
    // is a type of input archive the & operator is defined similar to >>.
    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) //这里面写需要序列话和反序列化的字段
    {
        ar & m_kvDB;
        ar & m_lastRequestId;
    }

    std::string getSnapshotData() {
        std::stringstream ss;
        boost::archive::text_oarchive oa(ss);
        oa << *this;
        return ss.str();
    }

    void parseFromString(const std::string &str) {
        std::stringstream ss(str);
        boost::archive::text_iarchive ia(ss);
        ia >> *this;
    }

    /////////////////serialiazation end ///////////////////////////////
};



