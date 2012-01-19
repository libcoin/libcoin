
#include "btcHTTP/Client.h"

#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <exception>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;

Client::Client() : _resolver(_io_service), _socket(_io_service), _completion_handler(*this) {}

Client::Client(io_service& io_service) : _resolver(io_service), _socket(io_service), _completion_handler(*this) { }

void Client::async_post(std::string url, std::string content, ClientCompletionHandler& handler, Headers headers) {
    // Form the request. We specify the "Connection: close" header so that the
    // server will close the socket after transmitting the response. This will
    // allow us to treat all data up until the EOF as the content.
    // find protocol
    size_t proto_sep = url.find("://");
    string protocol = "http";
    if (proto_sep != string::npos) {
        protocol = url.substr(0, proto_sep);
        url = url.substr(proto_sep + 3); // remove the protocol from the url
    }
    if (protocol != "http")
        throw string("Error: Only the HTTP protocol is suppored!");
    
    size_t slash = url.find("/");
    string server = url.substr(0, slash); // this will be the entire url if no slash is present
    size_t colon = server.find(":");
    string host = server.substr(0, colon);
    unsigned short port = 80;
    if (colon != string::npos)
        port = lexical_cast<unsigned short>(server.substr(colon + 1));
    string path = "/";
    if(slash != string::npos)
        path = url.substr(slash);
    std::ostream request_stream(&_request);
    request_stream << "POST " << path << " HTTP/1.0\r\n";
    request_stream << "Host: " << server << "\r\n";
    if(!headers.count("Content-Type"))
        request_stream << "Content-Type: application/json\r\n";
    for(Headers::iterator header = headers.begin(); header != headers.end(); ++header)
        request_stream << header->first << ": " << header->second << "\r\n";    
    request_stream << "Accept: */*\r\n";
    request_stream << "Content-Length: " << content.size() << "\r\n";
    request_stream << "Connection: close\r\n";
    request_stream << "\r\n";
    
    request_stream << content;

    // Start an asynchronous resolve to translate the server and service names
    // into a list of endpoints.
    tcp::resolver::query query(host, lexical_cast<string>(port));
    _resolver.async_resolve(query, bind(&Client::handle_resolve, this, placeholders::error, placeholders::iterator));
}

void Client::async_get(std::string url, ClientCompletionHandler& handler, Headers headers) {
    // Form the request. We specify the "Connection: close" header so that the
    // server will close the socket after transmitting the response. This will
    // allow us to treat all data up until the EOF as the content.
    // find protocol
    size_t proto_sep = url.find("://");
    string protocol = "http";
    if (proto_sep != string::npos) {
        protocol = url.substr(0, proto_sep);
        url = url.substr(proto_sep + 3); // remove the protocol from the url
    }
    if (protocol != "http")
        throw string("Error: Only the HTTP protocol is suppored!");
    
    size_t slash = url.find("/");
    string server = url.substr(0, slash); // this will be the entire url if no slash is present
    size_t colon = server.find(":");
    string host = server.substr(0, colon);
    unsigned short port = 80;
    if (colon != string::npos)
        port = lexical_cast<unsigned short>(server.substr(colon + 1));
    string path = "/";
    if(slash != string::npos)
        path = url.substr(slash);
    std::ostream request_stream(&_request);
    request_stream << "GET " << path << " HTTP/1.0\r\n";
    request_stream << "Host: " << server << "\r\n";
    for(Headers::iterator header = headers.begin(); header != headers.end(); ++header)
        request_stream << header->first << ": " << header->second << "\r\n";    
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n";
    request_stream << "\r\n";
    
    // Start an asynchronous resolve to translate the server and service names
    // into a list of endpoints.
    tcp::resolver::query query(host, lexical_cast<string>(port));
    _resolver.async_resolve(query, bind(&Client::handle_resolve, this, placeholders::error, placeholders::iterator));
}

void Client::handle_resolve(const system::error_code& err, tcp::resolver::iterator endpoint_iterator) {
    if (!err) {
        // Attempt a connection to the first endpoint in the list. Each endpoint
        // will be tried until we successfully establish a connection.
        tcp::endpoint endpoint = *endpoint_iterator;
        _socket.async_connect(endpoint, bind(&Client::handle_connect, this, placeholders::error, ++endpoint_iterator));
    }
    else {
        _completion_handler(err);
    }
}

void Client::handle_connect(const system::error_code& err, tcp::resolver::iterator endpoint_iterator) {
    if (!err) {
        // The connection was successful. Send the request.
        boost::asio::async_write(_socket, _request, bind(&Client::handle_write_request, this, placeholders::error));
    }
    else if (endpoint_iterator != tcp::resolver::iterator()) {
        // The connection failed. Try the next endpoint in the list.
        _socket.close();
        tcp::endpoint endpoint = *endpoint_iterator;
        _socket.async_connect(endpoint, bind(&Client::handle_connect, this, placeholders::error, ++endpoint_iterator));
    }
    else {
        _completion_handler(err);
    }
}

void Client::handle_write_request(const system::error_code& err) {
    if (!err) {
        // Read the response status line. The response_ streambuf will
        // automatically grow to accommodate the entire line. The growth may be
        // limited by passing a maximum size to the streambuf constructor.
        async_read_until(_socket, _response, "\r\n", bind(&Client::handle_read_status_line, this, placeholders::error));
    }
    else {
        _completion_handler(err);
    }
}

void Client::handle_read_status_line(const system::error_code& err) {
    if (!err) {
        // Check that response is OK.
        std::istream response_stream(&_response);
        std::string http_version;
        response_stream >> http_version;
        unsigned int status_code;
        response_stream >> status_code;
        _reply.status = (Reply::status_type)status_code;
        std::string status_message;
        std::getline(response_stream, status_message);
        if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
            std::cout << "Invalid response\n";
            return;
        }
        //        if (status_code != 200) {
        //            std::cout << "Response returned with status code ";
        //            std::cout << status_code << "\n";
        //            return;
        //        }
        
        // Read the response headers, which are terminated by a blank line.
        async_read_until(_socket, _response, "\r\n\r\n", bind(&Client::handle_read_headers, this, placeholders::error));
    }
    else {
        _completion_handler(err);
    }
}

void Client::handle_read_headers(const system::error_code& err) {
    if (!err) {
        // Process the response headers.
        std::istream response_stream(&_response);
        std::string header;
        while (std::getline(response_stream, header) && header != "\r") {
            size_t colon = header.find(": ");
            string key = header.substr(0, colon);
            if (colon != string::npos) {
                string value = header.substr(colon + 2); // skip colon and whitespace
                _reply.headers[key] = value;
            }
        }
        
        // Store whatever content we already have to output.
        stringstream ss;
        ss << &_response;
        _reply.content += ss.str();
        
        // Start reading remaining data until EOF.
        async_read(_socket, _response, transfer_at_least(1), bind(&Client::handle_read_content, this, placeholders::error));
    }
    else {
        _completion_handler(err);
    }
}

void Client::handle_read_content(const boost::system::error_code& err) {
    if (!err) {
        // Write all of the data that has been read so far.
        stringstream ss;
        ss << &_response;
        _reply.content += ss.str();
        
        // Continue reading remaining data until EOF.
        async_read(_socket, _response, transfer_at_least(1), bind(&Client::handle_read_content, this, placeholders::error));
    }
    else {
        _completion_handler(err);
    }
}
