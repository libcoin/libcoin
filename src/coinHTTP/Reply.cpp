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

#include <coinHTTP/Reply.h>
#include <coinHTTP/RPC.h>

#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/bind.hpp>

using namespace std;
using namespace boost;
using namespace asio;

namespace status_strings {
    
    const string ok =
    "HTTP/1.1 200 OK\r\n";
    const string created =
    "HTTP/1.1 201 Created\r\n";
    const string accepted =
    "HTTP/1.1 202 Accepted\r\n";
    const string no_content =
    "HTTP/1.1 204 No Content\r\n";
    const string multiple_choices =
    "HTTP/1.1 300 Multiple Choices\r\n";
    const string moved_permanently =
    "HTTP/1.1 301 Moved Permanently\r\n";
    const string moved_temporarily =
    "HTTP/1.1 302 Moved Temporarily\r\n";
    const string not_modified =
    "HTTP/1.1 304 Not Modified\r\n";
    const string bad_request =
    "HTTP/1.1 400 Bad Request\r\n";
    const string unauthorized =
    "HTTP/1.1 401 Unauthorized\r\n";
    const string forbidden =
    "HTTP/1.1 403 Forbidden\r\n";
    const string not_found =
    "HTTP/1.1 404 Not Found\r\n";
    const string internal_server_error =
    "HTTP/1.1 500 Internal Server Error\r\n";
    const string not_implemented =
    "HTTP/1.1 501 Not Implemented\r\n";
    const string bad_gateway =
    "HTTP/1.1 502 Bad Gateway\r\n";
    const string service_unavailable =
    "HTTP/1.1 503 Service Unavailable\r\n";
    const string gateway_timeout =
    "HTTP/1.1 504 Gateway Timeout\r\n";
    
    const_buffer to_buffer(Reply::Status status) {
        switch (status) {
            case Reply::ok:
                return buffer(ok);
            case Reply::created:
                return buffer(created);
            case Reply::accepted:
                return buffer(accepted);
            case Reply::no_content:
                return buffer(no_content);
            case Reply::multiple_choices:
                return buffer(multiple_choices);
            case Reply::moved_permanently:
                return buffer(moved_permanently);
            case Reply::moved_temporarily:
                return buffer(moved_temporarily);
            case Reply::not_modified:
                return buffer(not_modified);
            case Reply::bad_request:
                return buffer(bad_request);
            case Reply::unauthorized:
                return buffer(unauthorized);
            case Reply::forbidden:
                return buffer(forbidden);
            case Reply::not_found:
                return buffer(not_found);
            case Reply::internal_server_error:
                return buffer(internal_server_error);
            case Reply::not_implemented:
                return buffer(not_implemented);
            case Reply::bad_gateway:
                return buffer(bad_gateway);
            case Reply::service_unavailable:
                return buffer(service_unavailable);
            case Reply::gateway_timeout:
                return buffer(gateway_timeout);
            default:
                return buffer(internal_server_error);
        }
    }
    
} // namespace status_strings

namespace misc_strings {
    
    const char name_value_separator[] = { ':', ' ' };
    const char crlf[] = { '\r', '\n' };
    
} // namespace misc_strings

vector<const_buffer> Reply::to_buffers() {
    vector<const_buffer> buffers;
    buffers.push_back(status_strings::to_buffer(_status));
    for (Headers::iterator h = _headers.begin(); h != _headers.end(); ++h) {
        buffers.push_back(buffer(h->first));
        buffers.push_back(buffer(misc_strings::name_value_separator));
        buffers.push_back(buffer(h->second));
        buffers.push_back(buffer(misc_strings::crlf));
    }
    buffers.push_back(buffer(misc_strings::crlf));
    buffers.push_back(buffer(_content));
    return buffers;
}

namespace stock_replies {
    
    const char ok[] = "";
    const char created[] =
    "<html>"
    "<head><title>Created</title></head>"
    "<body><h1>201 Created</h1></body>"
    "</html>";
    const char accepted[] =
    "<html>"
    "<head><title>Accepted</title></head>"
    "<body><h1>202 Accepted</h1></body>"
    "</html>";
    const char no_content[] =
    "<html>"
    "<head><title>No Content</title></head>"
    "<body><h1>204 Content</h1></body>"
    "</html>";
    const char multiple_choices[] =
    "<html>"
    "<head><title>Multiple Choices</title></head>"
    "<body><h1>300 Multiple Choices</h1></body>"
    "</html>";
    const char moved_permanently[] =
    "<html>"
    "<head><title>Moved Permanently</title></head>"
    "<body><h1>301 Moved Permanently</h1></body>"
    "</html>";
    const char moved_temporarily[] =
    "<html>"
    "<head><title>Moved Temporarily</title></head>"
    "<body><h1>302 Moved Temporarily</h1></body>"
    "</html>";
    const char not_modified[] =
    "<html>"
    "<head><title>Not Modified</title></head>"
    "<body><h1>304 Not Modified</h1></body>"
    "</html>";
    const char bad_request[] =
    "<html>"
    "<head><title>Bad Request</title></head>"
    "<body><h1>400 Bad Request</h1></body>"
    "</html>";
    const char unauthorized[] =
    "<html>"
    "<head><title>Unauthorized</title></head>"
    "<body><h1>401 Unauthorized</h1></body>"
    "</html>";
    const char forbidden[] =
    "<html>"
    "<head><title>Forbidden</title></head>"
    "<body><h1>403 Forbidden</h1></body>"
    "</html>";
    const char not_found[] =
    "<html>"
    "<head><title>Not Found</title></head>"
    "<body><h1>404 Not Found</h1></body>"
    "</html>";
    const char internal_server_error[] =
    "<html>"
    "<head><title>Internal Server Error</title></head>"
    "<body><h1>500 Internal Server Error</h1></body>"
    "</html>";
    const char not_implemented[] =
    "<html>"
    "<head><title>Not Implemented</title></head>"
    "<body><h1>501 Not Implemented</h1></body>"
    "</html>";
    const char bad_gateway[] =
    "<html>"
    "<head><title>Bad Gateway</title></head>"
    "<body><h1>502 Bad Gateway</h1></body>"
    "</html>";
    const char service_unavailable[] =
    "<html>"
    "<head><title>Service Unavailable</title></head>"
    "<body><h1>503 Service Unavailable</h1></body>"
    "</html>";
    const char gateway_timeout[] =
    "<html>"
    "<head><title>Gateway Timeout</title></head>"
    "<body><h1>504 Gateway Timeout</h1></body>"
    "</html>";
    
    string to_string(Reply::Status status) {
        switch (status) {
            case Reply::ok:
                return ok;
            case Reply::created:
                return created;
            case Reply::accepted:
                return accepted;
            case Reply::no_content:
                return no_content;
            case Reply::multiple_choices:
                return multiple_choices;
            case Reply::moved_permanently:
                return moved_permanently;
            case Reply::moved_temporarily:
                return moved_temporarily;
            case Reply::not_modified:
                return not_modified;
            case Reply::bad_request:
                return bad_request;
            case Reply::unauthorized:
                return unauthorized;
            case Reply::forbidden:
                return forbidden;
            case Reply::not_found:
                return not_found;
            case Reply::internal_server_error:
                return internal_server_error;
            case Reply::not_implemented:
                return not_implemented;
            case Reply::bad_gateway:
                return bad_gateway;
            case Reply::service_unavailable:
                return service_unavailable;
            case Reply::gateway_timeout:
                return gateway_timeout;
            default:
                return internal_server_error;
        }
    }
    
} // namespace stock_replies

void Reply::status(Reply::Status s) {
    _status = s;
    _content = stock_replies::to_string(_status);
    _headers["Content-Length"] = boost::lexical_cast<string>(_content.size());
    _headers["Content-Type"] = "text/html";
}

void Reply::setContentAndMime(const std::string& content, const std::string& mime) {
    _status = ok;
    _headers["Content-Length"] = lexical_cast<string>(content.size());
    _headers["Content-Type"] = mime;
    _content = content;
}

void Reply::dispatch(const CompletionHandler& handler) {
    if (_method)
        _method->dispatch(boost::bind(&Reply::exec, this, handler), _request);
    else
        handler();
}

void Reply::exec(const CompletionHandler& handler) {
    try {
        RPC rpc(_request);
        
        try {
            if (!_method)
                throw rpc.response(RPC::method_not_found);
            json_spirit::Value result = (*_method)(rpc.params(), false, _request);
            json_spirit::Object response = rpc.result_response(result);
            setContentAndMime(write(response), "application/json");
        }
        catch (json_spirit::Object& err) {
            throw rpc.error_response(err);
        }
        catch (...) {
            throw rpc.response(RPC::internal_error);
        }
    }
    catch (json_spirit::Object& err_response) {
        setContentAndMime(write(err_response), "application/json");
    }
    handler();
}

