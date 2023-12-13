cmake_minimum_required(VERSION 3.8)

project(netput)

option(ABSL_PROPAGATE_CXX_STD "" ON)
option(ABSL_ENABLE_INSTALL "" ON)
option(gRPC_INSTALL "" OFF)
option(gRPC_BUILD_TESTS "" OFF)

add_subdirectory(grpc)

add_library(netput STATIC netput.cpp)

target_include_directories(netput PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(example)