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

#include <sstream>
#include <coinHTTP/RequestParser.h>
#include <coinHTTP/Request.h>

using namespace std;
using namespace boost;

RequestParser::RequestParser() : _state(method_start) {
}

void RequestParser::reset() {
    _state = method_start;
    
    _method.clear();
    _uri.clear();
    _version_major = _version_minor = 0;
    _payload.clear();
    
    _name.clear();
    _value.clear();
    _length = 0;

}

tribool RequestParser::consume(Request& req, char input)
{
    switch (_state) {
        case method_start:
            if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
                return false;
            }
            else {
                _state = method;
                _method.push_back(input);
                return indeterminate;
            }
        case method:
            if (input == ' ') {
                req.setMethod(_method);
                _method.clear();
                _state = uri;
                return indeterminate;
            }
            else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
                reset();
                return false;
            }
            else {
                _method.push_back(input);
                return indeterminate;
            }
        case uri_start:
            if (is_ctl(input)) {
                reset();
                return false;
            }
            else {
                _state = uri;
                _uri.push_back(input);
                return indeterminate;
            }
        case uri:
            if (input == ' ') {
                string uri;
                if (urlDecode(_uri, uri))
                    req.setURI(uri);
                else
                    return false;
                _uri.clear();
                _state = http_version_h;
                return indeterminate;
            }
            else if (is_ctl(input)) {
                reset();
                return false;
            }
            else {
                _uri.push_back(input);
                return indeterminate;
            }
        case http_version_h:
            if (input == 'H') {
                _state = http_version_t_1;
                return indeterminate;
            }
            else {
                reset();
                return false;
            }
        case http_version_t_1:
            if (input == 'T') {
                _state = http_version_t_2;
                return indeterminate;
            }
            else {
                reset();
                return false;
            }
        case http_version_t_2:
            if (input == 'T') {
                _state = http_version_p;
                return indeterminate;
            }
            else {
                reset();
                return false;
            }
        case http_version_p:
            if (input == 'P') {
                _state = http_version_slash;
                return indeterminate;
            }
            else {
                reset();
                return false;
            }
        case http_version_slash:
            if (input == '/') {
                _version_major = 0;
                _version_minor = 0;
                _state = http_version_major_start;
                return indeterminate;
            }
            else {
                reset();
                return false;
            }
        case http_version_major_start:
            if (is_digit(input)) {
                _version_major = _version_major * 10 + input - '0';
                _state = http_version_major;
                return indeterminate;
            }
            else {
                reset();
                return false;
            }
        case http_version_major:
            if (input == '.') {
                _state = http_version_minor_start;
                return indeterminate;
            }
            else if (is_digit(input)) {
                _version_major = _version_major * 10 + input - '0';
                return indeterminate;
            }
            else {
                reset();
                return false;
            }
        case http_version_minor_start:
            if (is_digit(input)) {
                _version_minor = _version_minor * 10 + input - '0';
                _state = http_version_minor;
                return indeterminate;
            }
            else {
                reset();
                return false;
            }
        case http_version_minor:
            if (input == '\r') {
                req.setVersion(_version_major, _version_minor);
                _state = expecting_newline_1;
                return indeterminate;
            }
            else if (is_digit(input)) {
                _version_minor = _version_minor * 10 + input - '0';
                return indeterminate;
            }
            else {
                reset();
                return false;
            }
        case expecting_newline_1:
            _length = 0;
            if (input == '\n') {
                _state = header_line_start;
                return indeterminate;
            }
            else {
                reset();
                return false;
            }
        case header_line_start:
            if (input == '\r') {
                _state = expecting_newline_3;
                return indeterminate;
            }
            else if (!req.headers().empty() && (input == ' ' || input == '\t')) {
                _state = header_lws;
                return indeterminate;
            }
            else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
                reset();
                return false;
            }
            else {
                _name.clear();                
                _name.push_back(tolower(input));
                _state = header_name;
                return indeterminate;
            }
        case header_lws:
            if (input == '\r') {
                _state = expecting_newline_2;
                return indeterminate;
            }
            else if (input == ' ' || input == '\t') {
                return indeterminate;
            }
            else if (is_ctl(input)) {
                reset();
                return false;
            }
            else {
                _state = header_value;
                _value.clear();
                _value.push_back(tolower(input));
                return indeterminate;
            }
        case header_name:
            if (input == ':') {
                _state = space_before_header_value;
                return indeterminate;
            }
            else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
                reset();
                return false;
            }
            else {
                _name.push_back(tolower(input));
                return indeterminate;
            }
        case space_before_header_value:
            if (input == ' ') {
                _state = header_value;
                _value.clear();
                return indeterminate;
            }
            else {
                reset();
                return false;
            }
        case header_value:
            if (input == '\r') {
                req.addHeader(_name, _value);
                if (_name == "content-length") {
                    istringstream(_value) >> _length;
                }
                _state = expecting_newline_2;
                return indeterminate;
            }
            else if (is_ctl(input)) {
                reset();
                return false;
            }
            else {
                _value.push_back(tolower(input));
                return indeterminate;
            }
        case expecting_newline_2:
            if (input == '\n') {
                _state = header_line_start;
                return indeterminate;
            }
            else {
                reset();
                return false;
            }
        case expecting_newline_3:
            if (input == '\n') {
                if(_length) {
                    _state = payload;
                    _payload.clear();
                    return indeterminate;
                }
                else {
                    req.setContent(_payload);
                    reset();
                    return true;
                }
            }
            else
                return false;
        case payload:
            _payload.push_back(input);
            if(_payload.size() < _length)
                return indeterminate;
            else {
                req.setContent(_payload);
                _payload.clear();
                reset();
                return true;
            }
        default:
            return false;
    }
}

bool RequestParser::is_char(int c) {
    return c >= 0 && c <= 127;
}

bool RequestParser::is_ctl(int c) {
    return (c >= 0 && c <= 31) || (c == 127);
}

bool RequestParser::is_tspecial(int c) {
    switch (c) {
        case '(': case ')': case '<': case '>': case '@':
        case ',': case ';': case ':': case '\\': case '"':
        case '/': case '[': case ']': case '?': case '=':
        case '{': case '}': case ' ': case '\t':
            return true;
        default:
            return false;
    }
}

bool RequestParser::is_digit(int c) {
    return c >= '0' && c <= '9';
}

bool RequestParser::urlDecode(const std::string& in, std::string& out) const {
    out.clear();
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%') {
            if (i + 3 <= in.size()) {
                int value = 0;
                std::istringstream is(in.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    out += static_cast<char>(value);
                    i += 2;
                }
                else {
                    return false;
                }
            }
            else {
                return false;
            }
        }
        else if (in[i] == '+') {
            out += ' ';
        }
        else {
            out += in[i];
        }
    }
    return true;
}
