cmake_minimum_required(VERSION 3.8)

project(netput)

option(ABSL_PROPAGATE_CXX_STD "" ON)
option(ABSL_ENABLE_INSTALL "" ON)
option(gRPC_INSTALL "" OFF)
option(gRPC_BUILD_TESTS "" OFF)

add_subdirectory(capnproto)

#add_library(netput STATIC netput.cpp netput.pb.h netput.pb.cc netput.grpc.pb.h netput.grpc.pb.cc)

#target_include_directories(netput PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/uuid_v4)

#target_link_libraries(netput PUBLIC grpc++)