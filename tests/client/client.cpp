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

#include <boost/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/assign.hpp>
#include <boost/asio.hpp>

#include <fstream>


using namespace std;
using namespace boost;
using namespace json_spirit;

class Response {
    Response& operator= (const Response& resp);
public:
    Response(const Request& request) : _request(request), _method(NULL) {}
    
    /// The status of the reply.
    enum Status {
        ok = 200,
        created = 201,
        accepted = 202,
        no_content = 204,
        multiple_choices = 300,
        moved_permanently = 301,
        moved_temporarily = 302,
        not_modified = 304,
        bad_request = 400,
        unauthorized = 401,
        forbidden = 403,
        not_found = 404,
        internal_server_error = 500,
        not_implemented = 501,
        bad_gateway = 502,
        service_unavailable = 503,
        gateway_timeout = 504
    };
    
    void status(Status s);
    
    Status status() const { return _status; }
    
    std::string& content() {
        return _content;
    }
    
    size_t content_length() const {
        return _content.size();
    }
    
    Headers& headers() {
        return _headers;
    }
    
    void setContentAndMime(const std::string& content, const std::string& mime);
        
    /// reset the reply (used for keep_alive)
    void reset() {
        _headers.clear();
        _content.clear();
    }
    
    /// Convert the reply into a vector of buffers. The buffers do not own the
    /// underlying memory blocks, therefore the reply object must remain valid and
    /// not be changed until the write operation has completed.
    std::vector<boost::asio::const_buffer> to_buffers();
    
private:
    /// The status
    Status _status;
    
    /// The Request generating this response
    const Request& _request;
    
    /// The headers to be included in the response.
    Headers _headers;
    
    /// The content to be sent in the response.
    std::string _content;
};

/// Interface class handling a response
class ResponseHandler {
public:
    virtual void operator()(Response& response) = 0;
};

/// A request consists of a request line, request headers and a body (for POST and PUT)
class Request : public boost::enable_shared_from_this<Connection>, private boost::noncopyable {
public:
    Request() {}
    Request(string request_line, Headers headers, string body = "") : _request_line(request_line), _headers(headers), _body(body) {}

    const string& request_line() const { return _request_line; }
    
    const Headers& headers() const { return _headers; }
    
    bool virtual_host() const {
        Headers::const_iterator h = _headers.find("host");
        return (h != _headers.end());
    }
    
    const string& body() const { return _body; }

protected:
    string _request_line;
    Headers _headers;
    string _body;

private:
    void handle_resolve(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_connect(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_handshake(const boost::system::error_code& error);
    void handle_write_request(const boost::system::error_code& err);
    void handle_read_status_line(const boost::system::error_code& err, std::size_t bytes_transferred);
    void handle_read_headers(const boost::system::error_code& err, std::size_t bytes_transferred);
    void handle_read_content(const boost::system::error_code& err, std::size_t bytes_transferred);
    
private:
    boost::asio::io_service _io_service;
    boost::asio::ssl::context _context;
    bool _secure;
    boost::asio::ip::tcp::resolver _resolver;
    boost::asio::ip::tcp::socket _socket;
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> _ssl_socket;
    boost::asio::streambuf _request;
    boost::asio::streambuf _response;
};

class Post : public Request {
public:
    Post(string resource, string body);
};

class Get : public Request {
public:
    Get(string resource) : Request() {
        _request_line = "GET " + resource + " HTTP/1.1";
    }:
};

typedef boost::shared_ptr<Request> request_ptr;

class Client {
public:
    Client(boost::asio::io_service& io_service, std::string default_host) : _io_service(io_service), _host(default_host) {
        
    }
    
    void request(request_ptr req, string host = "") {
        
        
        if (req->host() == "") {
            req->host(_host);
        }
        
        
    }
private:
    /// The default host
    std::string _host;
    /// The managed requests.
    std::set<request_ptr> _requests;
};

void response_handler(Response& response) {
    cout << "Status: " << response.status() << endl;
    cout << response.body() << endl;
}

int main(int argc, char* argv[]) {
    Client client("http://ceptacle.com");

    boost::asio::io_service io_service;
    client.request(new Get(io_service, "index.php", &response_handler));

    io_service.run();
    
    return 0;
}
