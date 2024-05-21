cmake_minimum_required(VERSION 3.22)
project(KvRAFT)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE "Debug")

# 设置项目可执行文件输出的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# 设置项目库文件输出的路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置项目编译头文件搜索路径 -I
include_directories(${PROJECT_SOURCE_DIR}/src/common/include)
include_directories(${PROJECT_SOURCE_DIR}/src/rpc/include)
include_directories(${PROJECT_SOURCE_DIR}/example)
include_directories(${PROJECT_SOURCE_DIR}/src/RaftCore/include)
include_directories(${PROJECT_SOURCE_DIR}/src/RaftRpcPro/include)
include_directories(${PROJECT_SOURCE_DIR}/src/raftClerk/include)
include_directories(${PROJECT_SOURCE_DIR}/src/fiber/include)

# 设置项目库文件搜索路径 -L
link_directories(${PROJECT_SOURCE_DIR}/lib)

# src是项目主要逻辑的代码 ， example中是实例实现的代码
add_subdirectory(example)
add_subdirectory(src)

set(SOURCES ${src_rpc} ${rpc_example} ${src_fiber}${raftsource} ${src_raftCore} ${src_raftRpcPro} ${src_raftClerk})
add_library(raft_main STATIC ${SOURCES}) 

target_link_libraries(raft_main muduo_net muduo_base pthread dl)

add_custom_target(format
        COMMAND bash ${PROJECT_SOURCE_DIR}/format.sh
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND echo "format done!"
)