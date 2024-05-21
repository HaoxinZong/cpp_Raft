#pragma once
#include "raftRpc.h"
#include<mutex>
#include <mutex>
#include <iostream>
#include <chrono>
#include <thread>
#include "ApplyMsg.h"
#include "util.h"
#include <vector>
#include "config.h"
#include <string>
#include <memory>
#include <cmath>
#include "boost/serialization/serialization.hpp"
#include "boost/any.hpp"
#include <boost/serialization/vector.hpp>
#include "Persister.h"
#include <boost/serialization/string.hpp>
#include"hook.h"
#include"fiber.h"
#include"timer.h"
#include"monsoon.h"
constexpr int Disconnected = 0; // 方便网络分区的时候debug，网络异常的时候为disconnected，只要网络正常就为AppNormal，防止matchIndex[]数组异常减小
constexpr int AppNormal = 1;
///////////////投票状态

constexpr int Killed = 0;
constexpr int Voted = 1;//本轮已经投过票了
constexpr int Expire = 2; //投票（消息、竞选者）过期
constexpr int Normal = 3;

class Raft : public raftRpcProctoc::raftRpc
{
private:
    std::mutex m_mtx;
    std::vector<std::shared_ptr<RaftRpc>> m_peers; //需要与其他raft节点通信，保存其他节点通信的rpc入口
    std::shared_ptr<Persister> m_persister; // 持久化层,负责raft数据的持久化
    int m_me;                               // raft 以集群启动，用这个来标记自己的编号
    int m_currentTerm;                      // 记录当前的Term
    int m_votedFor;                         // 记录当前给谁投票过
   
    std::vector<raftRpcProctoc::LogEntry> m_logs ;//日志条目数组，第一个日志记录的index值为1，
                                                  //包含了状态机要执行的指令集，以及收到领导时的任期号

    //所有节点持有的 易失性状态信息

    int m_commitIndex; // 最后一个已提交日志记录的index （初始值为0）//leader已提交至上层状态机的日志index  
    //当 Leader 将日志条目成功复制到大多数（或全部）节点时，它会将 commitIndex 提升到该日志条目的索引。
    //这表示 Leader 认为该日志已经被确认并提交，可以应用到状态机上，以使状态机的状态发生变化。  

    int m_lastApplied; // 最后一个已应用到上层状态机的日志记录的index （初始值为0）
    //在 Raft 中，除了 Leader 外的其他节点也会逐个地将已提交的日志应用到自己的状态机上。
    //一旦节点将日志应用到状态机上，就会将 lastApplied 提升到该日志条目的索引


    //Leader 节点持有的易失性的状态信息
    std::vector<int> m_nextIndex; // 该数组记录Leader即将发送给 每个**Follower的下一个日志记录的索引** ，初始值为Leader最新记录索引值+1
    std::vector<int> m_matchIndex; // 记录了每个Follower **已经复制的最后一条日志记录的索引**

    enum Status //raft节点的当前身份
    {
        Follower,
        Candidate,
        Leader
    };

    Status m_status ; //节点的当前身份


    std::shared_ptr<LockQueue<ApplyMsg>> applyChan;  //raft向kvserver 传递leader已commit ，但还没应用的日志
    //选举超时的时间
    std::chrono::_V2::system_clock::time_point m_lastResetElectionTime;
    //心跳超时，用于leader
    std::chrono::_V2::system_clock::time_point m_lastResetHeartBeatTime;

    //用于传入快照点
    //储存了快照中的最后一个日志的index和Term
    int m_lastSnapshotIncludeIndex; 
    int m_lastSnapshotIncludeTerm;
    //协程
    std::unique_ptr<monsoon::IOManager> m_ioManager  =nullptr;
public: //先写几个关键函数 ，其他的函数都是在函数中用到再写。

    // Leader 发起的日志同步 + 心跳rpc 
    void AppendEntries1(const raftRpcProctoc::AppendEntriesArgs *args, raftRpcProctoc::AppendEntriesReply *reply);

    // 持久化
    void persist();

    // 变成candidate后要让其他节点给自己投票
    void RequestVote(const raftRpcProctoc::RequestVoteArgs *args, raftRpcProctoc::RequestVoteReply *reply);

    // 请求其他节点的投票
    bool sendRequestVote(int server ,std::shared_ptr<raftRpcProctoc::RequestVoteArgs>args,
                         std::shared_ptr<raftRpcProctoc::RequestVoteReply>reply,std::shared_ptr<int>voteNum);
    
    bool sendAppendEntries(int server, std::shared_ptr<raftRpcProctoc::AppendEntriesArgs>args,
                           std::shared_ptr<raftRpcProctoc::AppendEntriesReply>reply,std::shared_ptr<int> appendNums);
    
    //几个定时器
    //定期向状态机 写入日志
    void applierTicker();

    //监控是否该发起选举
    void electionTimeOutTicker();

    //监控Leader是否该发起心跳
    void leaderHeartBeatTicker();

    //初始化
    void init(std::vector<std::shared_ptr<RaftRpc>>peers , int me ,std::shared_ptr<Persister> persister,
              std::shared_ptr<LockQueue<ApplyMsg>> applyCh);
    void readPersist(std::string data);

    std::string persistData();

    void doElection(); // 选举超时时间到后，开启一次选举。

    void doHeartBeat(); // 心跳超时时间到后，进行一次心跳

    std::vector<ApplyMsg> getApplyLogs(); // 获取已Apply的日志

    int getLastLogIndex(); //获取最后一个log的index

    int getLastLogTerm();

    void getLastLogIndexAndTerm(int *lastLogIndex, int *lastLogTerm); //获取m_logs中最后日志的index 和 term

    int getLogTermFromLogIndex(int logIndex);

    int getSlicesIndexFromLogIndex(int logIndex);

    void getPrevLogInfo(int server, int *preIndex, int *preTerm);//Leader 调用，得到对应server上最后一个日志条目的信息，用来准备为特定server发送的AE

    bool matchLog(int logIndex, int logTerm);
    void InstallSnapshot(const raftRpcProctoc::InstallSnapshotRequest *args,
                       raftRpcProctoc::InstallSnapshotResponse *reply);

    //重写基类方法 ，序列化 反序列化框架已经做完了 ，这里只需要调用本地服务即可
    void AppendEntries(google::protobuf::RpcController* controller,
                         const ::raftRpcProctoc::AppendEntriesArgs* request,
                         ::raftRpcProctoc::AppendEntriesReply* response, ::google::protobuf::Closure* done)override;

    void RequestVote(google::protobuf::RpcController *controller, const ::raftRpcProctoc::RequestVoteArgs *request,
                   ::raftRpcProctoc::RequestVoteReply *response, ::google::protobuf::Closure *done) override;

    void InstallSnapshot(google::protobuf::RpcController *controller,
                       const ::raftRpcProctoc::InstallSnapshotRequest *request,
                       ::raftRpcProctoc::InstallSnapshotResponse *response, ::google::protobuf::Closure *done) override;
    //rf.applyChan <- msg //不拿锁执行  可以单独创建一个线程执行，但是为了同意使用std:thread ，避免使用pthread_create，因此专门写一个函数来执行
    void pushMsgToKvServer(ApplyMsg msg); 

    //用于向raft集群中的leader提交新的操作
    void StartConses(Op command, int* newLogIndex, int* newLogTerm, bool* isLeader);
    
    int getNewCommandIndex();

    // GetState return currentTerm and whether this server believes it is the Leader.
    // 该函数将当前server的term返回，并让server判断自己是不是leader
    void GetState(int *term, bool *isLeader);

    // 从本地（persister）读出raft当前state的大小
    int GetRaftStateSize();

    // Snapshot the service says it has created a snapshot that has
    // all info up to and including index. this means the
    // service no longer needs the log through (and including)
    // that index. Raft should now trim its log as much as possible.
    // index代表是快照apply应用的index,而snapshot代表的是上层service传来的快照字节流，包括了Index之前的数据
    // 这个函数的目的是把安装到快照里的日志抛弃，并安装快照数据，同时更新快照下标，属于peers自身主动更新，与leader发送快照不冲突
    // 即服务层主动发起请求raft保存snapshot里面的数据，index是用来表示snapshot快照执行到了哪条命令
    void Snapshot(int index , std::string snapshot );

    bool CondInstallSnapshot(int lastIncludedTerm, int lastIncludedIndex, std::string snapshot);

    void leaderUpdateCommitIndex();

    void leaderSendSnapShot(int server);

    bool UpToDate(int index, int term);
private:
    // 具体来说，这个类包含了一些 Raft 节点的状态变量，如当前任期 m_currentTerm、已投票给的候选人ID m_votedFor、
    // 最后包含在快照中的日志索引和任期 m_lastSnapshotIncludeIndex 和 m_lastSnapshotIncludeTerm，
    //以及一些日志记录 m_logs 和一个 std::unordered_map。通过实现 serialize 方法，这个类可以被 Boost 库中的序列化功能序列化和反序列化。
    //在 serialize 方法中，通过 & 操作符对类的成员变量进行序列化和反序列化操作。当需要将对象序列化为二进制数据时，
    //输出归档（output archive）会将对象的状态写入到归档中；当需要从二进制数据中恢复对象状态时，输入归档（input archive）会将数据读取到对象的成员变量中。
    //这个类的功能可以用于将 Raft 节点的状态持久化到磁盘上，以及从磁盘上的持久化数据中恢复 Raft 节点的状态。
    //这在实现 Raft 协议中非常重要，因为它确保了即使在节点重启或崩溃的情况下，节点的状态也能够得到恢复，保证了系统的一致性和可靠性。
    class BoostPersistRaftNode
    {
    public:
        friend class boost::serialization::access;
        template<class Archive>
        void serialize(Archive &ar ,const unsigned int version)
        {
            ar & m_currentTerm;
            ar & m_votedFor;
            ar & m_lastSnapshotIncludeIndex;
            ar & m_lastSnapshotIncludeTerm;
            ar & m_logs;
        }
        int m_currentTerm;
        int m_votedFor;
        int m_lastSnapshotIncludeIndex;
        int m_lastSnapshotIncludeTerm;
        std::vector<std::string> m_logs;
        std::unordered_map<std::string,int> umap;
    };
};



