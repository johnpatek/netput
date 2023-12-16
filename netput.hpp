#ifndef _NETPUT_HPP_
#define _NETPUT_HPP_

#include <functional>
#include <memory>
#include <string>

namespace netput
{
    enum input_state
    {
        released,
        pressed
    };

    enum mouse_button
    {
        left,
        middle,
        right,
        x1,
        x2
    };

    struct mouse_button_state_mask
    {
        input_state left;
        input_state middle;
        input_state right;
        input_state x1;
        input_state x2;
    };

    enum window_event
    {
        shown,
        hidden,
        exposed,
        moved,
        resized,
        minimized,
        maximized,
        restored,
        mouse_enter,
        mouse_leave,
        focus_gained,
        focus_lost,
    };

    namespace internal
    {
        class client;
        class server;
    }

    class client
    {
    public:
        client(const std::string &host, uint16_t port);

        ~client() = default;

        void connect(const uint8_t *buffer, size_t size);

        void disconnect();

        void send_keyboard(uint64_t timestamp, uint32_t window_id, input_state state, bool repeat, uint32_t key_code);

        void send_mouse_motion(uint64_t timestamp, uint32_t window_id, mouse_button_state_mask state_mask, int32_t x, int32_t y, int32_t relative_x, int32_t relative_y);

        void send_mouse_button(uint64_t timestamp, uint32_t window_id, mouse_button button, input_state state, bool double_click, int32_t x, int32_t y);

        void send_mouse_wheel(uint64_t timestamp, uint32_t window_id, int32_t x, int32_t y);

        void send_mouse_wheel(uint64_t timestamp, uint32_t window_id, int32_t x, int32_t y, float precise_x, float precise_y);

        void send_window(uint64_t timestamp, uint32_t window_id, window_event type, int32_t arg1, int32_t arg2);

        std::string &get_session_id() const;

    private:
        std::unique_ptr<internal::client> _client;
    };

    class service;

    class server
    {
    public:
        server(const std::string &host, uint16_t port);
        ~server() = default;
        void serve();
        void shutdown();
        void handle_connect(const std::function<bool(const uint8_t *, size_t)> &connect_handler, std::function<bool(const std::string &)> session_handler);
        void handle_disconnect(const std::function<bool(const std::string &)> &disconnect_handler);
        void handle_keyboard(const std::function<void(const std::string &, uint64_t, uint32_t, input_state, bool, uint32_t)> &keyboard_handler);
        void handle_mouse_motion(const std::function<void(const std::string &, uint64_t, uint32_t, mouse_button_state_mask, int32_t, int32_t, int32_t, int32_t)> &mouse_motion_handler);
        void handle_mouse_button(const std::function<void(const std::string &, uint64_t, uint32_t, mouse_button, input_state, bool, int32_t, int32_t)> &mouse_button_handler);
        void handle_mouse_wheel(const std::function<void(const std::string &, uint64_t, uint32_t, int32_t, int32_t, float, float)> &mouse_wheel_handler);
        void handle_window(const std::function<void(const std::string &)> &window_handler);
    private:
        std::unique_ptr<internal::server> _server;
    };
}

#endif