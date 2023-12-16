#include "netput.hpp"
#include "netput.pb.h"
#include "netput.grpc.pb.h"

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

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
    namespace internal
    {
        class client
        {
        public:
            client(const std::string &address)
            {
                _channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
                _stub = std::unique_ptr<Netput::Stub>(
                    new Netput::Stub(_channel));
            }

            ~client() = default;

            std::pair<bool, std::string> connect(
                const ConnectRequest &request,
                ConnectResponse *response)
            {
                grpc::ClientContext context;
                std::pair<bool, std::string> result;
                grpc::Status status;

                result.first = true;
                status = _stub->Connect(&context, request, response);
                if (status.error_code() != grpc::StatusCode::OK)
                {
                    result.first = false;
                    result.second = status.error_message();
                }
                return result;
            }

            std::pair<bool, std::string> disconnect(
                const DisconnectRequest &request,
                DisconnectResponse *response)
            {
                grpc::ClientContext context;
                std::pair<bool, std::string> result;
                grpc::Status status;

                result.first = true;
                status = _stub->Disconnect(&context, request, response);
                if (status.error_code() != grpc::StatusCode::OK)
                {
                    result.first = false;
                    result.second = status.error_message();
                }
                return result;
            }

            std::pair<bool, std::string> event(
                const EventRequest &request,
                EventResponse *response)
            {
                grpc::ClientContext context;
                std::pair<bool, std::string> result;
                grpc::Status status;

                result.first = true;
                status = _stub->Event(&context, request, response);
                if (status.error_code() != grpc::StatusCode::OK)
                {
                    result.first = false;
                    result.second = status.error_message();
                }
                return result;
            }

        private:
            std::shared_ptr<grpc::Channel> _channel;
            std::unique_ptr<Netput::Stub> _stub;
        };

        class server final : public netput::internal::Netput::Service
        {
        public:
            server(const std::string &address)
            {
                grpc::ServerBuilder server_builder;
                server_builder.AddListeningPort(address, grpc::InsecureServerCredentials());
                server_builder.RegisterService(this);
                _server = server_builder.BuildAndStart();
            }

            ~server() = default;

            void serve()
            {
                if (_server)
                {
                    _server->Wait();
                }
            }

            void shutdown()
            {
                if (_server)
                {
                    _server->Shutdown();
                }
            }

            grpc::Status Connect(
                grpc::ServerContext *context,
                const netput::internal::ConnectRequest *request,
                netput::internal::ConnectResponse *response) override
            {
                const std::string &user_data = request->userdata();
                grpc::Status result;
                std::string session_id;

                result = grpc::Status::OK;

                if (_connect_handler)
                {
                    if (!_connect_handler(reinterpret_cast<const uint8_t *>(user_data.data()), user_data.size()))
                    {
                        result = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "connection rejected");
                    }
                }

                if (result.error_code() == grpc::StatusCode::OK)
                {
                    session_id = generate_session_id();
                    if (_session_handler)
                    {
                        if (!_session_handler(session_id))
                        {
                            result = grpc::Status(grpc::StatusCode::UNAVAILABLE, "failed to handle session");
                        }
                    }
                }

                if (result.error_code() == grpc::StatusCode::OK)
                {
                    response->set_sessionid(generate_session_id());
                }

                return result;
            }

            grpc::Status Disconnect(
                grpc::ServerContext *context,
                const netput::internal::DisconnectRequest *request,
                netput::internal::DisconnectResponse *response)
            {
                grpc::Status result;

                result = grpc::Status::OK;
                if (_disconnect_handler)
                {
                    if (!_disconnect_handler(request->sessionid()))
                    {
                        result = grpc::Status(grpc::StatusCode::UNAVAILABLE, "failed to handle session disconnect");
                    }
                }
                return result;
            }

            grpc::Status keyboardEvent(
                const std::string &session_id,
                const netput::internal::KeyboardEvent &keyboard_event)
            {
                const input_state state = (keyboard_event.state() == netput::internal::KeyState::Up) ? input_state::released : input_state::pressed;
                if (_keyboard_handler)
                {
                    _keyboard_handler(
                        session_id,
                        keyboard_event.timestamp(),
                        keyboard_event.windowid(),
                        state,
                        keyboard_event.repeat(),
                        keyboard_event.keycode());
                }
                return grpc::Status::OK;
            }

            grpc::Status mouseMotionEvent(
                const std::string &session_id,
                const netput::internal::MouseMotionEvent &mouse_motion_event)
            {
                return grpc::Status::OK;
            }

            grpc::Status Event(
                grpc::ServerContext *context,
                const netput::internal::EventRequest *request,
                netput::internal::EventResponse *response)
            {
                grpc::Status status;
                switch (request->type())
                {
                case netput::internal::EventType::KeyboardType:
                    status = keyboardEvent(request->sessionid(), request->keyboard());
                    break;
                case netput::internal::EventType::MouseMotionType:
                    status = mouseMotionEvent(request->sessionid(), request->mousemotion());
                    break;
                case netput::internal::EventType::MouseButtonType:
                    break;
                case netput::internal::EventType::MouseWheelType:
                    break;
                case netput::internal::EventType::WindowType:
                    break;
                default:
                    status = grpc::Status(grpc::StatusCode::OUT_OF_RANGE, "invalid event type");
                    break;
                }
                return status;
            }

            // TODO: maybe move these, probably not
            std::function<bool(const uint8_t *, size_t)> _connect_handler;
            std::function<bool(const std::string &)> _session_handler;
            std::function<bool(const std::string &)> _disconnect_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, input_state, bool, uint32_t)> _keyboard_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, mouse_button_state_mask, int32_t, int32_t, int32_t, int32_t)> _mouse_motion_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, mouse_button, input_state, bool, int32_t, int32_t)> _mouse_button_handler;
            std::function<void(const std::string &, uint64_t, uint32_t, int32_t, int32_t, float, float)> _mouse_wheel_handler;
            std::function<void(const std::string &)> _window_handler;

        private:
            std::string generate_session_id()
            {
                return _uuid_generator.getUUID().str();
            }

            UUIDv4::UUIDGenerator<std::mt19937_64> _uuid_generator;

            std::unique_ptr<grpc::Server> _server;
        };
    }

    client::client(const std::string &host, uint16_t port)
    {
        _client = std::unique_ptr<internal::client>(new internal::client(make_address(host, port)));
    }

    server::server(const std::string &host, uint16_t port)
    {
        _server = std::unique_ptr<internal::server>(new internal::server(make_address(host, port)));
    }

    void server::serve()
    {
        _server->serve();
    }

    void server::shutdown()
    {
        _server->shutdown();
    }

    void server::handle_connect(const std::function<bool(const uint8_t *, size_t)> &connect_handler, std::function<bool(const std::string &)> session_handler)
    {
        _server->_connect_handler = connect_handler;
        _server->_session_handler = session_handler;
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