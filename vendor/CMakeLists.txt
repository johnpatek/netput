add_subdirectory(capnproto)

if(NETPUT_TESTS)
    option(SDL_STATIC "" ON)
    option(SDL_STATIC_PIC "" ON)
    add_subdirectory(SDL)
    add_subdirectory(sigfn)
endif()