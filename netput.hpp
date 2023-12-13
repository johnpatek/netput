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
        client(const std::string &host, uint16_t port);

    private:
        std::unique_ptr<void> _channel;
    };

    enum mouse_state
    {
        Press,
        Release
    };

    struct mouse_button_state_mask
    {
        
    };

    class server
    {
    public:
        server(const std::string &host, uint16_t port);
        ~server() = default;

        void serve();
        void shutdown();

        void handle_keyboard(const std::function<void(uint64_t, uint32_t, int, bool, uint32_t)> &handler);
        void handle_mouse_motion(const std::function<void(uint64_t, uint32_t, int, int32_t, int32_t, int32_t, int32_t)> &handler);
        void handle_mouse_button(const std::function<void(uint64_t, uint32_t, int, int, bool, int32_t, int32_t)> &handler);
        void handle_mouse_wheel(const std::function<void()>& handler);
        void handle_window(const std::function<void()>& handler);

    private:
        bool _active;
        std::unique_ptr<void> _service;
        std::unique_ptr<void> _server;
    };
}

#endif