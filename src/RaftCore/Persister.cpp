#include"Persister.h"
#include"util.h"

void Persister::Save(const std::string raftstate,const std::string snapshot){
    std::lock_guard<std::mutex> lg(mtx);
    
    std::ofstream outfile;
    m_raftStateOutStream << raftstate;
    m_snapshotOutStream << snapshot;
}

std::string Persister::ReadSnapshot()
{
    std::lock_guard<std::mutex> lg(mtx);
    if(m_snapshotOutStream.is_open()){
        m_snapshotOutStream.close();
    }
    Defer ec1([this]()->void{
        this->m_snapshotOutStream.open(snapshotFile);
    }); //这个变量后生成 ， 会先销毁

    std::fstream ifs(snapshotFile ,std::ios_base::in);
    if(!ifs.good()){
        return "";
    }
    std::string snapshot;
    ifs >> snapshot;
    ifs.close();
    return snapshot;
}

void Persister::SaveRaftState(const std::string &data) {
    std::lock_guard<std::mutex> lg(mtx);
    // 将raftstate和snapshot写入本地文件
    m_raftStateOutStream << data;
}

long long Persister::RaftStateSize() {
    std::lock_guard<std::mutex> lg(mtx);

    return m_raftStateSize;
}

// std::string Persister::ReadRaftState() {
//     std::lock_guard<std::mutex> lg(mtx);

//     std::ifstream ifs(raftStateFile);
//     if (!ifs.good()) {
//         return ""; // 文件打开失败，返回空字符串
//     }

//     std::stringstream buffer;
//     buffer << ifs.rdbuf(); // 将文件内容读取到 stringstream 中
//     std::string raftState = buffer.str();

//     // 关闭文件流
//     ifs.close();

//     return raftState;
// }


std::string Persister::ReadRaftState() {
    std::lock_guard<std::mutex> lg(mtx);

    std::fstream ifs(raftStateFile, std::ios_base::in);
    if (!ifs.good()) {
        return "";
    }
    std::string snapshot;
    ifs >> snapshot;
    ifs.close();
    return snapshot;
}


Persister::Persister(int me) : raftStateFile("raftstatePersist" + std::to_string(me) + ".txt"),
                               snapshotFile("snapshotPersist" + std::to_string(me) + ".txt"), m_raftStateSize(0) 
{
    // std::fstream file(raftStateFile, std::ios::out | std::ios::trunc);
    // if (file.is_open()) {
    //     file.close();
    // }
    // file = std::fstream(snapshotFile, std::ios::out | std::ios::trunc);
    // if (file.is_open()) {
    //     file.close();
    // }
    // m_raftStateOutStream.open(raftStateFile);
    // m_snapshotOutStream.open(snapshotFile);
    m_raftStateOutStream.open(raftStateFile,std::ios::in|std::ios::out|std::ios::trunc);
    if(!m_raftStateOutStream.is_open()){
        std::cerr << "Failed to open file: " << raftStateFile << std::endl;
    }
    m_snapshotOutStream.open(snapshotFile,std::ios::in|std::ios::out|std::ios::trunc);
    if (!m_snapshotOutStream.is_open()) {
        std::cerr << "Failed to open file: " << snapshotFile << std::endl;
    }
}

Persister::~Persister() {
    if (m_raftStateOutStream.is_open()) {
        m_raftStateOutStream.close();
    }
    if (m_snapshotOutStream.is_open()) {
        m_snapshotOutStream.close();
    }
}
