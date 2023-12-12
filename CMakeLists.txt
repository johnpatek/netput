cmake_minimum_required(VERSION 3.8)

project(netput)

option(ABSL_PROPAGATE_CXX_STD "" ON)
option(gRPC_INSTALL "" OFF)
option(gRPC_BUILD_TESTS "" OFF)
add_subdirectory(grpc)