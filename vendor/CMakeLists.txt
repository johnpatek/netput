option(BUILD_TESTING "" OFF)
add_subdirectory(capnproto)

if(NETPUT_TESTS)
    add_subdirectory(json11)
    option(SDL_STATIC "" ON)
    option(SDL_STATIC_PIC "" ON)
    add_subdirectory(SDL)
endif()