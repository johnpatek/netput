#include "netput.capnp.h"
#include "netput.hpp"

#ifdef _WIN32
#pragma comment(lib,"WS2_32.lib")
#endif

#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <kj/async.h>

#include <functional>
#include <iostream>
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

    static rpc::InputState input_state_to_rpc(input_state state)
    {
        return (state == input_state::pressed) ? rpc::InputState::PRESSED : rpc::InputState::RELEASED;
    }

    static mouse_button mouse_button_from_rpc(rpc::MouseButton button)
    {
        mouse_button result;
        switch (button)
        {
        case rpc::MouseButton::LEFT:
            result = mouse_button::left;
            break;
        case rpc::MouseButton::MIDDLE:
            result = mouse_button::middle;
            break;
        case rpc::MouseButton::RIGHT:
            result = mouse_button::right;
            break;
        case rpc::MouseButton::X1:
            result = mouse_button::x1;
            break;
        case rpc::MouseButton::X2:
            result = mouse_button::x2;
            break;
        }
        return result;
    }

    static rpc::MouseButton mouse_button_to_rpc(mouse_button button)
    {
        rpc::MouseButton result;
        switch (button)
        {
        case mouse_button::left:
            result = rpc::MouseButton::LEFT;
            break;
        case mouse_button::middle:
            result = rpc::MouseButton::MIDDLE;
            break;
        case mouse_button::right:
            result = rpc::MouseButton::RIGHT;
            break;
        case mouse_button::x1:
            result = rpc::MouseButton::X1;
            break;
        case mouse_button::x2:
            result = rpc::MouseButton::X2;
            break;
        }
        return result;
    }

    static window_event window_event_from_rpc(rpc::WindowEventType event)
    {
        const std::unordered_map<rpc::WindowEventType, window_event> map = {
            {rpc::WindowEventType::SHOWN_TYPE, window_event::shown},
            {rpc::WindowEventType::HIDDEN_TYPE, window_event::hidden},
            {rpc::WindowEventType::EXPOSED_TYPE, window_event::exposed},
            {rpc::WindowEventType::MOVED_TYPE, window_event::moved},
            {rpc::WindowEventType::RESIZED_TYPE, window_event::resized},
            {rpc::WindowEventType::MINIMIZED_TYPE, window_event::minimized},
            {rpc::WindowEventType::MAXIMIZED_TYPE, window_event::maximized},
            {rpc::WindowEventType::RESTORED_TYPE, window_event::restored},
            {rpc::WindowEventType::MOUSE_ENTER_TYPE, window_event::mouse_enter},
            {rpc::WindowEventType::MOUSE_LEAVE_TYPE, window_event::mouse_leave},
            {rpc::WindowEventType::FOCUS_GAINED_TYPE, window_event::focus_gained},
            {rpc::WindowEventType::FOCUS_LOST_TYPE, window_event::focus_lost},
        };
        return map.at(event);
    }

    static rpc::WindowEventType window_event_to_rpc(window_event event)
    {
        const std::unordered_map<window_event, rpc::WindowEventType> map = {
            {window_event::shown, rpc::WindowEventType::SHOWN_TYPE},
            {window_event::hidden, rpc::WindowEventType::HIDDEN_TYPE},
            {window_event::exposed, rpc::WindowEventType::EXPOSED_TYPE},
            {window_event::moved, rpc::WindowEventType::MOVED_TYPE},
            {window_event::resized, rpc::WindowEventType::RESIZED_TYPE},
            {window_event::minimized, rpc::WindowEventType::MINIMIZED_TYPE},
            {window_event::maximized, rpc::WindowEventType::MAXIMIZED_TYPE},
            {window_event::restored, rpc::WindowEventType::RESTORED_TYPE},
            {window_event::mouse_enter, rpc::WindowEventType::MOUSE_ENTER_TYPE},
            {window_event::mouse_leave, rpc::WindowEventType::MOUSE_LEAVE_TYPE},
            {window_event::focus_gained, rpc::WindowEventType::FOCUS_GAINED_TYPE},
            {window_event::focus_lost, rpc::WindowEventType::FOCUS_LOST_TYPE},
        };
        return map.at(event);
    }

    namespace internal
    {
        class client
        {
        public:
            client(const std::string &address)
            {
                _rpc_client = std::make_unique<capnp::EzRpcClient>(address);
                _main = std::make_unique<netput::rpc::Netput::Client>(_rpc_client->getMain<netput::rpc::Netput::Client>());
            }

            ~client() = default;

            client(const client &copy) = delete;

            client(client &&move) = default;

            void connect(const uint8_t *buffer, size_t size)
            {
                auto request = _main->connectRequest();
                auto builder = request.initRequest();
                auto user_data_builder = builder.initUserData(size);
                if (size > 0)
                {
                    std::memcpy(user_data_builder.begin(), buffer, size);
                }
                auto promise = request.send();
                auto reader = promise.wait(_rpc_client->getWaitScope());
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
                auto reader = promise.wait(_rpc_client->getWaitScope());
                if (reader.hasResponse() && reader.getResponse().hasError())
                {
                    throw std::runtime_error(std::string("error returned from server: ") + reader.getResponse().getError().cStr());
                }
            }

            void push(const std::function<void(netput::rpc::Event::Info::Builder &)> &build_function)
            {
                auto request = _main->pushRequest();
                auto builder = request.initEvent();
                builder.setSessionId(_session_id);
                auto info_builder = builder.initInfo();
                build_function(info_builder);
                auto promise = request.send();
                promise.wait(_rpc_client->getWaitScope());
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
                    mouse_motion_state_builder.setMiddle(input_state_to_rpc(state_mask.middle));
                    mouse_motion_state_builder.setRight(input_state_to_rpc(state_mask.right));
                    mouse_motion_state_builder.setX1(input_state_to_rpc(state_mask.x1));
                    mouse_motion_state_builder.setX2(input_state_to_rpc(state_mask.x2));
                    mouse_motion_builder.setX(x);
                    mouse_motion_builder.setY(y);
                    mouse_motion_builder.setRelativeX(relative_x);
                    mouse_motion_builder.setRelativeY(relative_y);
                };
                push(build_function);
            }

            void send_mouse_button(uint64_t timestamp, uint32_t window_id, mouse_button button, input_state state, bool double_click, int32_t x, int32_t y)
            {
                const std::function<void(netput::rpc::Event::Info::Builder &)> &build_function = [&](netput::rpc::Event::Info::Builder &builder)
                {
                    auto mouse_button_builder = builder.initMouseButton();
                    mouse_button_builder.setTimestamp(timestamp);
                    mouse_button_builder.setWindowId(window_id);
                    mouse_button_builder.setState(input_state_to_rpc(state));
                    mouse_button_builder.setX(x);
                    mouse_button_builder.setY(y);
                };
                push(build_function);
            }

            void send_mouse_wheel(uint64_t timestamp, uint32_t window_id, int32_t x, int32_t y, float precise_x, float precise_y)
            {
                const std::function<void(netput::rpc::Event::Info::Builder &)> &build_function = [&](netput::rpc::Event::Info::Builder &builder)
                {
                    auto mouse_wheel_builder = builder.initMouseWheel();
                    mouse_wheel_builder.setTimestamp(timestamp);
                    mouse_wheel_builder.setWindowId(window_id);
                    mouse_wheel_builder.setX(x);
                    mouse_wheel_builder.setY(y);
                    mouse_wheel_builder.setPreciseX(x);
                    mouse_wheel_builder.setPreciseY(y);
                };
                push(build_function);
            }

            void send_window(uint64_t timestamp, uint32_t window_id, window_event type, int32_t arg1, int32_t arg2)
            {
                const std::function<void(netput::rpc::Event::Info::Builder &)> &build_function = [&](netput::rpc::Event::Info::Builder &builder)
                {
                    auto window_builder = builder.initWindow();
                    window_builder.setTimestamp(timestamp);
                    window_builder.setWindowId(window_id);
                    window_builder.setType(window_event_to_rpc(type));
                    window_builder.setArg1(arg1);
                    window_builder.setArg1(arg2);
                };
                push(build_function);
            }

            std::function<void(const std::string &)> _error_handler;

        private:
            std::unique_ptr<capnp::EzRpcClient> _rpc_client;
            std::unique_ptr<netput::rpc::Netput::Client> _main;
            std::string _session_id;
        };

        class service final : public netput::rpc::Netput::Server
        {
        public:
            service(
                const std::function<void(const rpc::ConnectRequest::Reader &, rpc::ConnectResponse::Builder &)> &connect_handler,
                const std::function<void(const rpc::Event::Reader &)> &push_handler,
                const std::function<void(const rpc::DisconnectRequest::Reader &, rpc::DisconnectResponse::Builder &)> &disconnect_handler) : _connect_handler(connect_handler),
                                                                                                                                             _push_handler(push_handler),
                                                                                                                                             _disconnect_handler(disconnect_handler)
            {
            }

            kj::Promise<void> connect(netput::rpc::Netput::Server::ConnectContext context) override
            {
                const netput::rpc::ConnectRequest::Reader reader = context.getParams().getRequest();
                netput::rpc::ConnectResponse::Builder builder = context.getResults().initResponse();
                _connect_handler(reader, builder);
                return kj::READY_NOW;
            }

            kj::Promise<void> push(netput::rpc::Netput::Server::PushContext context) override
            {
                const netput::rpc::Event::Reader reader = context.getParams().getEvent();
                _push_handler(reader);
                return kj::READY_NOW;
            }

            kj::Promise<void> disconnect(netput::rpc::Netput::Server::DisconnectContext context) override
            {
                const netput::rpc::DisconnectRequest::Reader reader = context.getParams().getRequest();
                netput::rpc::DisconnectResponse::Builder builder = context.getResults().initResponse();
                _disconnect_handler(reader, builder);
                return kj::READY_NOW;
            }

        private:
            std::function<void(const rpc::ConnectRequest::Reader &, rpc::ConnectResponse::Builder &)> _connect_handler;
            std::function<void(const rpc::Event::Reader &)> _push_handler;
            std::function<void(const rpc::DisconnectRequest::Reader &, rpc::DisconnectResponse::Builder &)> _disconnect_handler;
        };

        class server
        {
        public:
            server(const std::string &address)
            {
                const auto connect_handler = [&](const rpc::ConnectRequest::Reader &reader, rpc::ConnectResponse::Builder &builder)
                {
                    this->handle_connect(reader, builder);
                };
                const auto push_handler = [&](const rpc::Event::Reader &reader)
                {
                    this->handle_push(reader);
                };
                const auto disconnect_handler = [&](const rpc::DisconnectRequest::Reader &reader, rpc::DisconnectResponse::Builder &builder)
                {
                    this->handle_disconnect(reader, builder);
                };
                _rpc_server = std::make_unique<capnp::EzRpcServer>(
                    kj::heap<service>(connect_handler, push_handler, disconnect_handler), address);
            }

            ~server()
            {
                shutdown();
            }

            void serve()
            {
                kj::WaitScope &wait_scope = _rpc_server->getWaitScope();
                _promise_fulfiller = kj::heap<kj::PromiseCrossThreadFulfillerPair<void>>(
                    kj::newPromiseAndCrossThreadFulfiller<void>());
                _promise_fulfiller->promise.wait(wait_scope);
                _rpc_server.reset();
            }

            void shutdown()
            {
                _promise_fulfiller->fulfiller->fulfill();
            }

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
                bool success;
                success = true;
                if (_disconnect_handler)
                {
                    if (reader.hasSessionId())
                    {
                        success = _disconnect_handler(reader.getSessionId());
                    }
                    else
                    {
                        success = false;
                    }
                }

                if (!success)
                {
                    builder.setError("disconnect failed");
                }
            }

            // TODO: maybe move these, probably not
            std::function<std::pair<bool, std::string>(const uint8_t *, size_t)> _connect_handler;
            std::function<bool(const std::string &)> _disconnect_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, input_state, bool, uint32_t)> _keyboard_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, const mouse_button_state_mask &, int32_t, int32_t, int32_t, int32_t)> _mouse_motion_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, mouse_button, input_state, bool, int32_t, int32_t)> _mouse_button_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, int32_t, int32_t, float, float)> _mouse_wheel_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, window_event, int32_t, int32_t)> _window_handler;

        private:
            void handle_mouse_motion(
                const std::string &session_id,
                const netput::rpc::MouseMotionEvent::Reader &reader)
            {
                mouse_button_state_mask state_mask;
                state_mask.left = input_state_from_rpc(reader.getStateMask().getLeft());
                state_mask.middle = input_state_from_rpc(reader.getStateMask().getMiddle());
                state_mask.right = input_state_from_rpc(reader.getStateMask().getRight());
                state_mask.x1 = input_state_from_rpc(reader.getStateMask().getX1());
                state_mask.x2 = input_state_from_rpc(reader.getStateMask().getX2());
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
                if (_mouse_button_handler)
                {
                    _mouse_button_handler(
                        session_id,
                        reader.getTimestamp(),
                        reader.getWindowId(),
                        mouse_button_from_rpc(reader.getButton()),
                        input_state_from_rpc(reader.getState()),
                        reader.getDouble(),
                        reader.getX(),
                        reader.getY());
                }
            }

            void handle_mouse_wheel(
                const std::string &session_id,
                const netput::rpc::MouseWheelEvent::Reader &reader)
            {
                if (_mouse_wheel_handler)
                {
                    _mouse_wheel_handler(
                        session_id,
                        reader.getTimestamp(),
                        reader.getWindowId(),
                        reader.getX(),
                        reader.getY(),
                        reader.getPreciseX(),
                        reader.getPreciseY());
                }
            }

            void handle_keyboard(
                const std::string &session_id,
                const netput::rpc::KeyboardEvent::Reader &reader)
            {
                if (_keyboard_handler)
                {
                    _keyboard_handler(
                        session_id,
                        reader.getTimestamp(),
                        reader.getWindowId(),
                        input_state_from_rpc(
                            reader.getState()),
                        reader.getRepeat(),
                        reader.getKeyCode());
                }
            }

            void handle_window(
                const std::string &session_id,
                const netput::rpc::WindowEvent::Reader &reader)
            {
                if (_window_handler)
                {
                    _window_handler(
                        session_id,
                        reader.getTimestamp(),
                        reader.getWindowId(),
                        window_event_from_rpc(reader.getType()),
                        reader.getArg1(),
                        reader.getArg2());
                }
            }

            std::unique_ptr<capnp::EzRpcServer> _rpc_server;
            bool _active;
            kj::Own<kj::PromiseCrossThreadFulfillerPair<void>> _promise_fulfiller;
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
        _client->connect(buffer, size);
    }

    void client::disconnect()
    {
        _client->disconnect();
    }

    void client::send_keyboard(uint64_t timestamp, uint32_t window_id, input_state state, bool repeat, uint32_t key_code)
    {
        _client->send_keyboard(timestamp, window_id, state, repeat, key_code);
    }

    void client::send_mouse_motion(uint64_t timestamp, uint32_t window_id, const mouse_button_state_mask &state_mask, int32_t x, int32_t y, int32_t relative_x, int32_t relative_y)
    {
        _client->send_mouse_motion(timestamp, window_id, state_mask, x, y, relative_x, relative_y);
    }

    void client::send_mouse_button(uint64_t timestamp, uint32_t window_id, mouse_button button, input_state state, bool double_click, int32_t x, int32_t y)
    {
        _client->send_mouse_button(timestamp, window_id, button, state, double_click, x, y);
    }

    void client::send_mouse_wheel(uint64_t timestamp, uint32_t window_id, int32_t x, int32_t y)
    {
        _client->send_mouse_wheel(timestamp, window_id, x, y, static_cast<float>(x), static_cast<float>(y));
    }

    void client::send_mouse_wheel(uint64_t timestamp, uint32_t window_id, int32_t x, int32_t y, float precise_x, float precise_y)
    {
        _client->send_mouse_wheel(timestamp, window_id, x, y, precise_x, precise_y);
    }

    void client::send_window(uint64_t timestamp, uint32_t window_id, window_event type, int32_t arg1, int32_t arg2)
    {
        _client->send_window(timestamp, window_id, type, arg1, arg2);
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
        _server->serve();
    }

    void server::shutdown()
    {
        _server->shutdown();
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

    void server::handle_mouse_motion(const std::function<void(const std::string &, uint64_t, uint32_t, const mouse_button_state_mask &, int32_t, int32_t, int32_t, int32_t)> &mouse_motion_handler)
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

    void server::handle_window(const std::function<void(const std::string &, uint64_t, uint32_t, window_event, int32_t, int32_t)> &window_handler)
    {
        _server->_window_handler = window_handler;
    }
}