add_library(netput STATIC netput.capnp.h netput.capnp.c++ netput.cpp)

target_include_directories(netput PUBLIC ${NETPUT_INCLUDE})

target_link_libraries(netput PUBLIC capnp capnp-rpc kj)