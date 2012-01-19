
#include "btcHTTP/Reply.h"

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>

class ClientCompletionHandler {
public:
    virtual void operator()(const boost::system::error_code& err) = 0;
};

class Client : public ClientCompletionHandler
{
public:
    Client();
    Client(boost::asio::io_service& io_service);

    virtual void operator()(const boost::system::error_code& err) { _error = err; }
    
    /// Blocking post.
    Reply post(std::string url, std::string content, Headers headers = Headers()) {
        async_post(url, content, *this, headers);
        _io_service.run();
        if(_error)
            _reply.content += _error.message();
        // parse the reply
        return _reply;
    }
    
    /// Non-blocking post. Requires definition of a callback.
    void async_post(std::string url, std::string content, ClientCompletionHandler& handler, Headers headers = Headers());
    
    /// Blocking get.
    Reply get(std::string url, Headers headers = Headers()) {
        async_get(url, *this, headers);
        _io_service.run();
        // parse the reply
        return _reply;
    }
    
    /// Non-blocking get. Requires definition of a callback.
    void async_get(std::string url, ClientCompletionHandler& handler, Headers headers = Headers());
    

private:
    void handle_resolve(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_connect(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_write_request(const boost::system::error_code& err);
    void handle_read_status_line(const boost::system::error_code& err);
    void handle_read_headers(const boost::system::error_code& err);
    void handle_read_content(const boost::system::error_code& err);

private:
    boost::asio::io_service _io_service;
    boost::asio::ip::tcp::resolver _resolver;
    boost::asio::ip::tcp::socket _socket;
    boost::asio::streambuf _request;
    boost::asio::streambuf _response;
    Reply _reply;
    ClientCompletionHandler& _completion_handler;
    boost::system::error_code _error;
};