#include "netput.hpp"
#include "netput.pb.h"
#include "netput.grpc.pb.h"

namespace netput
{
    class server_impl final : public netput::internal::Netput::Service
    {
    public:
        server_impl(const std::string &host, uint16_t port)
        {

        }
        
        grpc::Status Connect(
            grpc::ServerContext* context, 
            const netput::internal::ConnectRequest* request, 
            netput::internal::ConnectResponse* response_writer) override
        {
            
        }

    private:
    };

}