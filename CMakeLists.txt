cmake_minimum_required(VERSION 3.8)

project(netput)

option(NETPUT_TESTS "" OFF)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_subdirectory(vendor)

set(NETPUT_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)

if(NETPUT_TESTS)
    add_subdirectory(test)
endif()