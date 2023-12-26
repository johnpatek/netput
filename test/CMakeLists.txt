add_executable(
    test 
    main.cpp
    test.cpp
    test.hpp)

target_link_libraries(test PRIVATE json11 netput SDL2-static)