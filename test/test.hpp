#ifndef TEST_HPP
#define TEST_HPP

#include <netput.hpp>

#include <iostream>
#include <sstream>
#include <thread>

#include <SDL.h>
#include <json11.hpp>

#ifndef __FUNCTION_NAME__
#ifdef WIN32 // WINDOWS
#define __FUNCTION_NAME__ __FUNCTION__
#else //*NIX
#define __FUNCTION_NAME__ __func__
#endif
#endif

#define TEST_ASSERT_FULL(COND, PATH, FUNC, LINE)               \
    if (!COND)                                                 \
    {                                                          \
        std::ostringstream error_stream;                       \
        error_stream << PATH << ":"                            \
                     << FUNC << ":"                            \
                     << " assert \"" << #COND << "\" failed."; \
        throw std::runtime_error(error_stream.str());          \
    }
#define TEST_ASSERT(COND) TEST_ASSERT_FULL((COND), __FILE__, __FUNCTION_NAME__, __LINE__)

namespace test
{
    const std::string localhost = "0.0.0.0";
    const std::string loopback = "127.0.0.1";
    const uint16_t port = 12345;
    const std::string valid_password = "valid-netput-password";
    const std::string invalid_password = "invalid-netput-password";

    enum usage
    {
        ping,
        mouse_motion,
        mouse_button,
        mouse_wheel,
        keyboard,
        window,
    };

    const std::string session_ids[] = {
        "ping-session-id",
        "mouse-motion-session-id",
        "mouse-wheel-session-id",
        "keyboard-session-id",
        "window-session-id",
    };

    void execute();

    std::vector<uint8_t> encode_connect_data(int usage, const std::string &password);
    std::pair<int, std::string> decode_connect_data(const uint8_t *buffer, size_t size);

    bool connect(std::unique_ptr<netput::client> &client, int usage, const std::string &password);
    void send_event(std::unique_ptr<netput::client> &client, const SDL_Event *event);
    bool disconnect(std::unique_ptr<netput::client> &client);

}

#endif