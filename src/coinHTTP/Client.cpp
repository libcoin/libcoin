/* -*-c++-*- libcoin - Copyright (C) 2012 Michael Gronager
 *
 * libcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * libcoin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libcoin.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <coinHTTP/Client.h>
#include <coinHTTP/Request.h>

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

Client::Client() : _context(_io_service, ssl::context::sslv23), _resolver(_io_service), _socket(_io_service), _ssl_socket(_io_service, _context), _reply(Request()) {
    _context.set_options(ssl::context::no_sslv2);
    //    _ssl_socket.set_verify_mode(ssl::verify_none);
    _context.set_verify_mode(ssl::context_base::verify_none);
}

Client::Client(io_service& io_service) : _context(io_service, ssl::context::sslv23), _resolver(io_service), _socket(io_service), _ssl_socket(io_service, _context), _reply(Request()) {
    _context.set_options(ssl::context::no_sslv2);
    //    _ssl_socket.set_verify_mode(ssl::verify_none);
    _context.set_verify_mode(ssl::context_base::verify_none);
}

void Client::async_post(std::string url, std::string content, ReplyHandler handler, Headers headers) {
    _reply_handler = handler;
    // Form the request. We specify the "Connection: close" header so that the
    // server will close the socket after transmitting the response. This will
    // allow us to treat all data up until the EOF as the content.
    // find protocol
    size_t proto_sep = url.find("://");
    string protocol = "http";
    if(isSecure()) protocol = "https";
    if (proto_sep != string::npos) {
        protocol = url.substr(0, proto_sep);
        url = url.substr(proto_sep + 3); // remove the protocol from the url
    }
    unsigned short port;
    if (protocol == "http") {
        setSecure(false);
        port = 80;
    }
    else if (protocol == "https") {
        setSecure(true);
        port = 443;
    }
    else
        throw string("Error: Only the HTTP and HTTPS protocols are suppored!");
    
    size_t slash = url.find("/");
    string server = url.substr(0, slash); // this will be the entire url if no slash is present
    size_t colon = server.find(":");
    string host = server.substr(0, colon);
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
    _resolver.async_resolve(query, boost::bind(&Client::handle_resolve, this, asio::placeholders::error, asio::placeholders::iterator));
}

void Client::async_get(std::string url, ReplyHandler handler, Headers headers) {
    _reply_handler = handler;
    // Form the request. We specify the "Connection: close" header so that the
    // server will close the socket after transmitting the response. This will
    // allow us to treat all data up until the EOF as the content.
    // find protocol
    size_t proto_sep = url.find("://");
    string protocol = "http";
    if(isSecure()) protocol = "https";
    if (proto_sep != string::npos) {
        protocol = url.substr(0, proto_sep);
        url = url.substr(proto_sep + 3); // remove the protocol from the url
    }
    unsigned short port;
    if (protocol == "http") {
        setSecure(false);
        port = 80;
    }
    else if (protocol == "https") {
        setSecure(true);
        port = 443;
    }
    else
        throw string("Error: Only the HTTP and HTTPS protocols are suppored!");
    
    size_t slash = url.find("/");
    string server = url.substr(0, slash); // this will be the entire url if no slash is present
    size_t colon = server.find(":");
    string host = server.substr(0, colon);
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
    _resolver.async_resolve(query, boost::bind(&Client::handle_resolve, this, asio::placeholders::error, asio::placeholders::iterator));
}

void Client::handle_resolve(const system::error_code& err, tcp::resolver::iterator endpoint_iterator) {
    if (!err) {
        // Attempt a connection to the first endpoint in the list. Each endpoint
        // will be tried until we successfully establish a connection.
        tcp::endpoint endpoint = *endpoint_iterator;
        if(isSecure())
            _ssl_socket.lowest_layer().async_connect(endpoint, boost::bind(&Client::handle_connect, this, asio::placeholders::error, ++endpoint_iterator));
        else
            _socket.async_connect(endpoint, boost::bind(&Client::handle_connect, this, asio::placeholders::error, ++endpoint_iterator));
    }
    else {
        handle_done(err);
    }
}

void Client::handle_connect(const system::error_code& err, tcp::resolver::iterator endpoint_iterator) {
    if (!err) {
        // The connection was successful. Send the handshake or request.
        if(isSecure())
            _ssl_socket.async_handshake(ssl::stream_base::client, boost::bind(&Client::handle_handshake, this, asio::placeholders::error));
        else
            boost::asio::async_write(_socket, _request, boost::bind(&Client::handle_write_request, this, asio::placeholders::error));
    }
    else if (endpoint_iterator != tcp::resolver::iterator()) {
        // The connection failed. Try the next endpoint in the list.
        _socket.close();
        tcp::endpoint endpoint = *endpoint_iterator;
        _socket.async_connect(endpoint, boost::bind(&Client::handle_connect, this, asio::placeholders::error, ++endpoint_iterator));
    }
    else {
        handle_done(err);
    }
}

void Client::handle_handshake(const boost::system::error_code& err) {
    if (!err) {
        // The connection was successful. Send the request.
        if(isSecure())
            boost::asio::async_write(_ssl_socket, _request, boost::bind(&Client::handle_write_request, this, asio::placeholders::error));
        else
            boost::asio::async_write(_socket, _request, boost::bind(&Client::handle_write_request, this, asio::placeholders::error));
    }
    else {
        std::cout << "Handshake failed: " << err.message() << "\n";

        handle_done(err);
    }    
}

void Client::handle_write_request(const system::error_code& err) {
    if (!err) {
        // Read the response status line. The response_ streambuf will
        // automatically grow to accommodate the entire line. The growth may be
        // limited by passing a maximum size to the streambuf constructor.
        if(isSecure())
            async_read_until(_ssl_socket, _response, "\r\n", boost::bind(&Client::handle_read_status_line, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
        else
            async_read_until(_socket, _response, "\r\n", boost::bind(&Client::handle_read_status_line, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
    else {
        handle_done(err);
    }
}

void Client::handle_read_status_line(const system::error_code& err, std::size_t bytes_transferred) {
    if (!err) {
        // Check that response is OK.
        std::istream response_stream(&_response);
        std::string http_version;
        response_stream >> http_version;
        unsigned int status_code;
        response_stream >> status_code;
        _reply.status((Reply::Status)status_code);
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
        if(isSecure())
            async_read_until(_ssl_socket, _response, "\r\n\r\n", boost::bind(&Client::handle_read_headers, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
        else
            async_read_until(_socket, _response, "\r\n\r\n", boost::bind(&Client::handle_read_headers, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
    else if (bytes_transferred == 0) {
        boost::system::error_code e;
        handle_done(e);
    }
    else {
        cout << "read_statusline" << endl;

        handle_done(err);
    }
}

void Client::handle_read_headers(const system::error_code& err, std::size_t bytes_transferred) {
    if (!err) {
        // Process the response headers.
        std::istream response_stream(&_response);
        std::string header;
        while (std::getline(response_stream, header) && header != "\r") {
            size_t colon = header.find(": ");
            string key = header.substr(0, colon);
            if (colon != string::npos) {
                string value = header.substr(colon + 2); // skip colon and whitespace
                _reply.headers()[key] = value;
            }
        }
        
        // Store whatever content we already have to output.
        stringstream ss;
        ss << &_response;
        _reply.content() += ss.str();
        
        // Start reading remaining data until EOF.
        if(isSecure())
            async_read(_ssl_socket, _response, transfer_at_least(1), boost::bind(&Client::handle_read_content, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
        else
            async_read(_socket, _response, transfer_at_least(1), boost::bind(&Client::handle_read_content, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
    else if (bytes_transferred == 0) {
        boost::system::error_code e;
        handle_done(e);
    }
    else {
        cout << "read_headers" << endl;
        handle_done(err);
    }
}

void Client::handle_read_content(const boost::system::error_code& err, std::size_t bytes_transferred) {
    if (!err) {
        // Write all of the data that has been read so far.
        stringstream ss;
        ss << &_response;
        _reply.content() += ss.str();
        
        // Continue reading remaining data until EOF.
        if(isSecure())
            async_read(_ssl_socket, _response, transfer_at_least(1), boost::bind(&Client::handle_read_content, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
        else
            async_read(_socket, _response, transfer_at_least(1), boost::bind(&Client::handle_read_content, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
    else if (bytes_transferred == 0) {
        boost::system::error_code e;
        handle_done(e);
    }
    else {
        cout << "read_content" << endl;
        handle_done(err);
    }
}

void Client::handle_done(const boost::system::error_code& err) {
    _error = err;
    _reply_handler(_reply);
}

