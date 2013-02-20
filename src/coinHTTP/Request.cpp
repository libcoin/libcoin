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

#include <coinHTTP/Request.h>

#include <coinHTTP/json/json_spirit.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/ip/address.hpp>

#include <string>

void Request::setMethod(const std::string method) {
    _method = method;
}

void Request::setURI(const std::string& uri) {
    _uri = uri;
}

void Request::setVersion(int major, int minor) {
    _version_major = major;
    _version_minor = minor;
}

void Request::setContent(const std::string& content) {
    _content = content;
}

void Request::addHeader(const std::string& field, const std::string& value) {
    _headers[field] = value;
}

std::string Request::basic_auth() const {
    if(_headers.count("authorization")) {
        std::string auth = _headers.find("authorization")->second;
        if (auth.length() > 9 && auth.substr(0,6) == "basic ")
            return auth.substr(6);
    }
    return "";
}

json_spirit::Object Request::error(ErrorCode e, const std::string message) {
    json_spirit::Object err;
    err.push_back(json_spirit::Pair("code", (int)e));
    if (message.size()) {
        err.push_back(json_spirit::Pair("message", message));
        return err;
    }
    switch (e) {
        case invalid_request:
            err.push_back(json_spirit::Pair("message", "Invalid Request."));
            return err;
        case method_not_found:
            err.push_back(json_spirit::Pair("message", "Method not found."));
            return err;
        case invalid_params:
            err.push_back(json_spirit::Pair("message", "Invalid params."));
            return err;
        case internal_error:
            err.push_back(json_spirit::Pair("message", "Internal error."));
            return err;
        case parse_error:
            err.push_back(json_spirit::Pair("message", "Parse error."));
            return err;
        default:
            err.push_back(json_spirit::Pair("message", "Server error."));
            return err;
    }
}

/// reset the request (used for keep_alive)
void Request::reset() {
    _method.clear();
    _uri.clear();
    _headers.clear();
    _content.clear();
}
