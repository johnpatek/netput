#include "test.hpp"

int main(int argc, char **argv)
{
    int result;
    try
    {
        result = 0;
        test::execute();
    }   
    catch(const std::exception& error)
    {
        result = 1;
        std::cerr << error.what() << std::endl;
    } 
    return result;
}