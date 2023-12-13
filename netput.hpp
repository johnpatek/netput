#ifndef _NETPUT_HPP_
#define _NETPUT_HPP_

#include <functional>
#include <memory>
#include <string>

namespace netput
{
    class client
    {
    public:
        client(const std::string& host, uint16_t port);
    private:
        std::unique_ptr<void> _handle;
    };

    class server
    {
    public:
        server(const std::string& host, uint16_t port);
    private:
        std::unique_ptr<void> _handle;
    };
}

#endif