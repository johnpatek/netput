#include "test.hpp"

void test::execute()
{
    std::unique_ptr<netput::server> server;
    std::thread server_thread;
    std::unique_ptr<netput::client> ping_client;
    bool ping_connected;
    bool ping_timeout;

    server_thread = std::thread(
        [&]()
        {
            server = std::make_unique<netput::server>(test::localhost, test::port);
            server->handle_connect(
                [&](const uint8_t *buffer, size_t size)
                {
                    std::pair<bool, std::string> result;
                    std::pair<int, std::string> connect_data;
                    connect_data = decode_connect_data(buffer, size);
                    if (connect_data.second.compare(test::valid_password) == 0)
                    {
                        result = std::make_pair(true, test::session_ids[connect_data.first]);
                    }
                    else
                    {
                        result = std::make_pair(false, "invalid password");
                    }
                    return result;
                });
            server->handle_disconnect(
                [&](const std::string &session_id)
                {
                    bool result;
                    result = false;
                    for (const std::string &item : test::session_ids)
                    {
                        if (item.compare(session_id) == 0)
                        {
                            result = true;
                        }
                    }
                    return result;
                });
            server->serve();
        });

    ping_client = std::make_unique<netput::client>(test::loopback, test::port);

    auto ping_start = std::chrono::steady_clock::now();
    do
    {
        ping_connected = test::connect(ping_client, test::usage::ping, test::valid_password);
        ping_timeout = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - ping_start) > std::chrono::seconds(5);
    } while (!ping_connected && !ping_timeout);
    TEST_ASSERT(ping_connected && !ping_timeout)
    TEST_ASSERT(test::disconnect(ping_client))
    
    
    server->shutdown();
    if (!server_thread.joinable())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    TEST_ASSERT(server_thread.joinable())
    server_thread.join();
    std::cerr << "exit" << std::endl;
}

std::vector<uint8_t> test::encode_connect_data(int usage, const std::string &password)
{
    json11::Json json;
    json11::Json::object object;
    std::string string;
    std::vector<uint8_t> result;

    object["usage"] = usage;
    object["password"] = password;
    json = json11::Json(object);
    string = json.dump();
    result.resize(string.size());
    SDL_memcpy(result.data(), string.data(), result.size());

    return result;
}

std::pair<int, std::string> test::decode_connect_data(const uint8_t *buffer, size_t size)
{
    const std::string string(reinterpret_cast<const char *>(buffer), size);
    std::string error;
    json11::Json json;
    json11::Json object;
    std::pair<int, std::string> result;

    json = json11::Json::parse(string, error);
    object = json.object_items();

    return std::make_pair(object["usage"].int_value(), object["password"].string_value());
}

bool test::connect(std::unique_ptr<netput::client> &client, int usage, const std::string &password)
{
    bool result;
    std::vector<uint8_t> connect_data;

    result = true;
    connect_data = test::encode_connect_data(usage, password);

    try
    {
        client->connect(connect_data.data(), connect_data.size());
    }
    catch (const std::exception &error)
    {
        result = false;
    }

    return result;
}

void test::send_event(std::unique_ptr<netput::client> &client, const SDL_Event *event)
{
    bool valid_event;
    valid_event = true;
    switch (event->type)
    {
    case SDL_MOUSEMOTION:
        /* code */
        break;
    
    default:
        valid_event = false;
        break;
    }
    TEST_ASSERT(valid_event)
}

bool test::disconnect(std::unique_ptr<netput::client> &client)
{
    bool result;
    result = true;
    try
    {
        client->disconnect();
    }
    catch (const std::exception &error)
    {
        result = false;
    }
    return result;
}