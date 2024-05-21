#include<mutex>
#include<fstream>

class Persister{
private:
    std::mutex mtx;
    std::string m_raftState;
    std::string m_snapshot;

    const std::string raftStateFile;
    const std::string snapshotFile;
    std::ofstream m_raftStateOutStream;
    std::ofstream m_snapshotOutStream;
    long long m_raftStateSize; 
public:
    void Save(std::string raftstate,std::string snapshot);
    std::string ReadSnapshot();
    void SaveRaftState(const std::string& date);
    long long RaftStateSize();
    std::string ReadRaftState();
    explicit Persister(int me);
    ~Persister();
};