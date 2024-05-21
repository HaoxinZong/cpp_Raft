
set(SRC_LIST run.cpp)


add_executable(raftStart ${SRC_LIST})
target_link_libraries(raftStart raft_main rpc_lib protobuf muduo_net muduo_base pthread )

set(SRC_LIST2 servicecaller.cpp)
add_executable(servicecaller ${src_raftClerk} ${SRC_LIST2} ${src_common})
target_link_libraries(servicecaller raft_main protobuf boost_serialization)