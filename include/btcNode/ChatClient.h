#ifndef CHATCLIENT_H
#define CHATCLIENT_H


#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>

class Endpoint;
class EndpointPool;

class ChatClient
{
public:
    ChatClient(boost::asio::io_service& io_service, const std::string& server, EndpointPool& endpointPool, const bool proxy = false);
    
private:
    void handle_resolve(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_connect(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_read_line(const boost::system::error_code& err, std::size_t bytes_transferred);    
    void handle_write_request(const boost::system::error_code& err);
    
    std::string encodeAddress(const Endpoint& addr);
    bool decodeAddress(std::string str, Endpoint& addr);
    
#pragma pack(push, 1)
    struct ircaddr {
        int ip;
        short port;
    };
#pragma pack(pop)
    
private:
    boost::asio::ip::tcp::resolver _resolver;
    boost::asio::ip::tcp::socket _socket;
    boost::asio::streambuf _send;
    boost::asio::streambuf _recv;
    
    enum {
        wait_for_notice,
        wait_for_motd,
        wait_for_ip,
        in_chat_room
    } _mode;
    
    std::string _my_name;
    bool _name_in_use;
    const std::string _server;
    EndpointPool& _endpointPool;
    bool _proxy;
};

#endif // CHATCLIENT_H