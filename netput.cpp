#include "netput.capnp.h"
#include "netput.hpp"

#include <capnp/ez-rpc.h>
#include <capnp/message.h>

#include <uuid_v4.h>

#include <functional>
#include <sstream>

static std::string make_address(const std::string &host, uint16_t port)
{
    std::stringstream address;
    address << host << ":" << port;
    return address.str();
}

namespace netput
{
    static input_state input_state_from_rpc(rpc::InputState state)
    {
        return (state == rpc::InputState::PRESSED) ? input_state::pressed : input_state::released;
    }

    static netput::rpc::InputState input_state_to_rpc(input_state state)
    {
        return (state == input_state::pressed) ? rpc::InputState::PRESSED : rpc::InputState::RELEASED;
    }

    namespace internal
    {
        class client
        {
        public:
            client(const std::string &address)
            {
                _client = std::make_unique<capnp::EzRpcClient>(address);
                _main = std::make_unique<netput::rpc::Netput::Client>(_client->getMain<netput::rpc::Netput::Client>());
            }

            ~client() = default;

            client(const client &copy) = delete;

            client(client &&move) = default;

            void connect(const uint8_t *buffer, size_t size)
            {
                auto request = _main->connectRequest();
                auto builder = request.initRequest();
                auto user_data_builder = builder.initUserData(size);
                std::memcpy(user_data_builder.begin(), buffer, size);
                auto promise = request.send();
                auto reader = promise.wait(_client->getWaitScope());
                if (!reader.hasResponse())
                {
                    throw std::runtime_error("failed to read response from server");
                }

                auto response = reader.getResponse();
                auto message = response.getMessage();
                if (message.isError())
                {
                    throw std::runtime_error(std::string("error returned from server: ") + message.getError().cStr());
                }

                _session_id = message.getSessionId();
            }

            void disconnect()
            {
                auto request = _main->disconnectRequest();
                auto builder = request.initRequest();
                builder.setSessionId(_session_id);
                auto promise = request.send();
                auto reader = promise.wait(_client->getWaitScope());
            }

            void push(const std::function<void(netput::rpc::Event::Info::Builder &)> &build_function)
            {
                auto request = _main->pushRequest();
                auto builder = request.initEvent();
                builder.setSessionId(_session_id);
                auto info_builder = builder.initInfo();
                build_function(info_builder);
                auto promise = request.send();
                promise.detach([&](const kj::Exception &error)
                               {
                    if(_error_handler)
                    {
                        _error_handler(error.getDescription());
                    } });
            }

            void send_keyboard(uint64_t timestamp, uint32_t window_id, input_state state, bool repeat, uint32_t key_code)
            {
                const std::function<void(netput::rpc::Event::Info::Builder &)> &build_function = [&](netput::rpc::Event::Info::Builder &builder)
                {
                    auto keyboard_builder = builder.initKeyboard();
                    keyboard_builder.setTimestamp(timestamp);
                    keyboard_builder.setWindowId(window_id);
                    keyboard_builder.setState(input_state_to_rpc(state));
                    keyboard_builder.setRepeat(repeat);
                    keyboard_builder.setKeyCode(key_code);
                };
                push(build_function);
            }

            void send_mouse_motion(uint64_t timestamp, uint32_t window_id, mouse_button_state_mask state_mask, int32_t x, int32_t y, int32_t relative_x, int32_t relative_y)
            {
                const std::function<void(netput::rpc::Event::Info::Builder &)> &build_function = [&](netput::rpc::Event::Info::Builder &builder)
                {
                    auto mouse_motion_builder = builder.initMouseMotion();
                    mouse_motion_builder.setTimestamp(timestamp);
                    mouse_motion_builder.setWindowId(window_id);
                    auto mouse_motion_state_builder = mouse_motion_builder.initStateMask();
                    mouse_motion_state_builder.setLeft(input_state_to_rpc(state_mask.left));
                };
                push(build_function);
            }

            void send_mouse_button(uint64_t timestamp, uint32_t window_id, mouse_button button, input_state state, bool double_click, int32_t x, int32_t y);

            void send_mouse_wheel(uint64_t timestamp, uint32_t window_id, int32_t x, int32_t y);

            void send_mouse_wheel(uint64_t timestamp, uint32_t window_id, int32_t x, int32_t y, float precise_x, float precise_y);

            void send_window(uint64_t timestamp, uint32_t window_id, window_event type, int32_t arg1, int32_t arg2);

            std::function<void(const std::string &)> _error_handler;

        private:
            std::unique_ptr<capnp::EzRpcClient> _client;
            std::unique_ptr<netput::rpc::Netput::Client> _main;
            std::string _session_id;
        };

        class server final : public netput::rpc::Netput::Server
        {
        public:
            server(const std::string &address) : _exit_channel(kj::newPromiseAndFulfiller<void>())
            {
            }

            ~server()
            {
                shutdown();
            }

            void serve()
            {
                kj::WaitScope &wait_scope = _rpc_server->getWaitScope();
                _active = true;
                _exit_channel.promise.wait(wait_scope);
            }

            void shutdown()
            {
                if (_active)
                {
                    _exit_channel.fulfiller->fulfill();
                }
            }

            kj::Promise<void> connect(netput::rpc::Netput::Server::ConnectContext context) override
            {
                const netput::rpc::ConnectRequest::Reader reader = context.getParams().getRequest();
                netput::rpc::ConnectResponse::Builder builder = context.getResults().initResponse();
                handle_connect(reader, builder);
                return kj::READY_NOW;
            }

            kj::Promise<void> push(netput::rpc::Netput::Server::PushContext context) override
            {
                const netput::rpc::Event::Reader reader = context.getParams().getEvent();
                handle_push(reader);
                return kj::READY_NOW;
            }

            kj::Promise<void> disconnect(netput::rpc::Netput::Server::DisconnectContext context) override
            {
                const netput::rpc::DisconnectRequest::Reader reader = context.getParams().getRequest();
                netput::rpc::DisconnectResponse::Builder builder = context.getResults().initResponse();
                handle_disconnect(reader, builder);
                return kj::READY_NOW;
            }

            // TODO: maybe move these, probably not
            std::function<std::pair<bool, std::string>(const uint8_t *, size_t)> _connect_handler;
            std::function<bool(const std::string &)> _disconnect_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, input_state, bool, uint32_t)> _keyboard_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, mouse_button_state_mask, int32_t, int32_t, int32_t, int32_t)> _mouse_motion_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, mouse_button, input_state, bool, int32_t, int32_t)> _mouse_button_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, int32_t, int32_t, float, float)> _mouse_wheel_handler;
            std::function<void(const std::string &)> _window_handler;

        private:
            void handle_connect(
                const netput::rpc::ConnectRequest::Reader &reader,
                netput::rpc::ConnectResponse::Builder &builder)
            {
                const uint8_t *user_data_buffer;
                size_t user_data_size;
                std::pair<bool, std::string> result;

                if (reader.hasUserData())
                {
                    const capnp::Data::Reader &user_data = reader.getUserData();
                    user_data_buffer = user_data.begin();
                    user_data_size = user_data.size();
                }
                else
                {
                    user_data_buffer = nullptr;
                    user_data_size = 0;
                }

                if (_connect_handler)
                {
                    result = _connect_handler(user_data_buffer, user_data_size);
                }
                else
                {
                    result.first = false;
                    result.second = "unimplemented connection handler";
                }

                if (result.first)
                {
                    builder.initMessage().setSessionId(result.second);
                }
                else
                {
                    builder.initMessage().setError(result.second);
                }
            }

            void handle_push(const netput::rpc::Event::Reader &reader)
            {
                const std::string session_id = reader.getSessionId();
                const netput::rpc::Event::Info::Reader info = reader.getInfo();
                switch (info.which())
                {
                case netput::rpc::Event::Info::MOUSE_MOTION:
                {
                    const netput::rpc::MouseMotionEvent::Reader mouse_motion_reader = info.getMouseMotion();
                    handle_mouse_motion(session_id, mouse_motion_reader);
                    break;
                }
                case netput::rpc::Event::Info::MOUSE_BUTTON:
                {
                    const netput::rpc::MouseButtonEvent::Reader mouse_button_reader = info.getMouseButton();
                    handle_mouse_button(session_id, mouse_button_reader);
                    break;
                }
                case netput::rpc::Event::Info::MOUSE_WHEEL:
                {
                    const netput::rpc::MouseWheelEvent::Reader mouse_wheel_reader = info.getMouseWheel();
                    handle_mouse_wheel(session_id, mouse_wheel_reader);
                    break;
                }
                case netput::rpc::Event::Info::KEYBOARD:
                {
                    const netput::rpc::KeyboardEvent::Reader keyboard_reader = info.getKeyboard();
                    handle_keyboard(session_id, keyboard_reader);
                    break;
                }
                case netput::rpc::Event::Info::WINDOW:
                {
                    const netput::rpc::WindowEvent::Reader window_reader = info.getWindow();
                    handle_window(session_id, window_reader);
                    break;
                }
                }
            }

            void handle_disconnect(
                const netput::rpc::DisconnectRequest::Reader &reader,
                netput::rpc::DisconnectResponse::Builder &builder)
            {
            }

            void handle_mouse_motion(
                const std::string &session_id,
                const netput::rpc::MouseMotionEvent::Reader &reader)
            {
                const netput::mouse_button_state_mask state_mask = {
                    .left = input_state_from_rpc(reader.getStateMask().getLeft()),
                    .middle = input_state_from_rpc(reader.getStateMask().getMiddle()),
                    .right = input_state_from_rpc(reader.getStateMask().getRight()),
                    .x1 = input_state_from_rpc(reader.getStateMask().getX1()),
                    .x2 = input_state_from_rpc(reader.getStateMask().getX2()),
                };
                if (_mouse_motion_handler)
                {
                    _mouse_motion_handler(
                        session_id,
                        reader.getTimestamp(),
                        reader.getWindowId(),
                        state_mask,
                        reader.getX(),
                        reader.getY(),
                        reader.getRelativeX(),
                        reader.getRelativeY());
                }
            }

            void handle_mouse_button(
                const std::string &session_id,
                const netput::rpc::MouseButtonEvent::Reader &reader)
            {
            }

            void handle_mouse_wheel(
                const std::string &session_id,
                const netput::rpc::MouseWheelEvent::Reader &reader)
            {
            }

            void handle_keyboard(
                const std::string &session_id,
                const netput::rpc::KeyboardEvent::Reader &reader)
            {
            }

            void handle_window(
                const std::string &session_id,
                const netput::rpc::WindowEvent::Reader &reader)
            {
            }

            std::string generate_session_id()
            {
                return _uuid_generator.getUUID().str();
            }

            std::unique_ptr<capnp::EzRpcServer> _rpc_server;
            kj::PromiseFulfillerPair<void> _exit_channel;
            bool _active;
            UUIDv4::UUIDGenerator<std::mt19937_64> _uuid_generator;
        };
    }

    client::client(const std::string &host, uint16_t port)
    {
        _client = std::unique_ptr<internal::client, std::function<void(internal::client *)>>(
            new internal::client(make_address(host, port)),
            [](internal::client *client)
            {
                delete client;
            });
    }

    void client::connect(const uint8_t *buffer, size_t size)
    {
    }

    void client::disconnect()
    {
    }

    void client::send_keyboard(uint64_t timestamp, uint32_t window_id, input_state state, bool repeat, uint32_t key_code)
    {
    }

    void client::send_mouse_motion(uint64_t timestamp, uint32_t window_id, mouse_button_state_mask state_mask, int32_t x, int32_t y, int32_t relative_x, int32_t relative_y)
    {
    }

    void client::send_mouse_button(uint64_t timestamp, uint32_t window_id, mouse_button button, input_state state, bool double_click, int32_t x, int32_t y)
    {
    }

    void client::send_mouse_wheel(uint64_t timestamp, uint32_t window_id, int32_t x, int32_t y)
    {
    }

    void client::send_mouse_wheel(uint64_t timestamp, uint32_t window_id, int32_t x, int32_t y, float precise_x, float precise_y)
    {
    }

    void client::send_window(uint64_t timestamp, uint32_t window_id, window_event type, int32_t arg1, int32_t arg2)
    {
    }

    server::server(const std::string &host, uint16_t port)
    {
        _server = std::unique_ptr<internal::server, std::function<void(internal::server *)>>(
            new internal::server(make_address(host, port)),
            [](internal::server *server)
            {
                delete server;
            });
    }

    void server::serve()
    {
    }

    void server::shutdown()
    {
    }

    void server::handle_connect(const std::function<std::pair<bool, std::string>(const uint8_t *, size_t)> &connect_handler)
    {
        _server->_connect_handler = connect_handler;
    }

    void server::handle_disconnect(const std::function<bool(const std::string &)> &disconnect_handler)
    {
        _server->_disconnect_handler = disconnect_handler;
    }

    void server::handle_keyboard(const std::function<void(const std::string &, uint64_t, uint32_t, input_state, bool, uint32_t)> &keyboard_handler)
    {
        _server->_keyboard_handler = keyboard_handler;
    }

    void server::handle_mouse_motion(const std::function<void(const std::string &, uint64_t, uint32_t, mouse_button_state_mask, int32_t, int32_t, int32_t, int32_t)> &mouse_motion_handler)
    {
        _server->_mouse_motion_handler = mouse_motion_handler;
    }

    void server::handle_mouse_button(const std::function<void(const std::string &, uint64_t, uint32_t, mouse_button, input_state, bool, int32_t, int32_t)> &mouse_button_handler)
    {
        _server->_mouse_button_handler = mouse_button_handler;
    }

    void server::handle_mouse_wheel(const std::function<void(const std::string &, uint64_t, uint32_t, int32_t, int32_t, float, float)> &mouse_wheel_handler)
    {
        _server->_mouse_wheel_handler = mouse_wheel_handler;
    }

    void server::handle_window(const std::function<void(const std::string &)> &window_handler)
    {
        _server->_window_handler = window_handler;
    }
}