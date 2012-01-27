
#include <sstream>
#include "btcHTTP/RequestParser.h"
#include "btcHTTP/Request.h"

using namespace std;
using namespace boost;

RequestParser::RequestParser() : _state(method_start) {
}

void RequestParser::reset() {
    _state = method_start;
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
                req.method.push_back(input);
                return indeterminate;
            }
        case method:
            if (input == ' ') {
                _state = uri;
                return indeterminate;
            }
            else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
                return false;
            }
            else {
                req.method.push_back(input);
                return indeterminate;
            }
        case uri_start:
            if (is_ctl(input)) {
                return false;
            }
            else {
                _state = uri;
                req.uri.push_back(input);
                return indeterminate;
            }
        case uri:
            if (input == ' ') {
                _state = http_version_h;
                return indeterminate;
            }
            else if (is_ctl(input)) {
                return false;
            }
            else {
                req.uri.push_back(input);
                return indeterminate;
            }
        case http_version_h:
            if (input == 'H') {
                _state = http_version_t_1;
                return indeterminate;
            }
            else {
                return false;
            }
        case http_version_t_1:
            if (input == 'T') {
                _state = http_version_t_2;
                return indeterminate;
            }
            else {
                return false;
            }
        case http_version_t_2:
            if (input == 'T') {
                _state = http_version_p;
                return indeterminate;
            }
            else {
                return false;
            }
        case http_version_p:
            if (input == 'P') {
                _state = http_version_slash;
                return indeterminate;
            }
            else {
                return false;
            }
        case http_version_slash:
            if (input == '/') {
                req.http_version_major = 0;
                req.http_version_minor = 0;
                _state = http_version_major_start;
                return indeterminate;
            }
            else {
                return false;
            }
        case http_version_major_start:
            if (is_digit(input)) {
                req.http_version_major = req.http_version_major * 10 + input - '0';
                _state = http_version_major;
                return indeterminate;
            }
            else {
                return false;
            }
        case http_version_major:
            if (input == '.') {
                _state = http_version_minor_start;
                return indeterminate;
            }
            else if (is_digit(input)) {
                req.http_version_major = req.http_version_major * 10 + input - '0';
                return indeterminate;
            }
            else {
                return false;
            }
        case http_version_minor_start:
            if (is_digit(input)) {
                req.http_version_minor = req.http_version_minor * 10 + input - '0';
                _state = http_version_minor;
                return indeterminate;
            }
            else {
                return false;
            }
        case http_version_minor:
            if (input == '\r') {
                _state = expecting_newline_1;
                return indeterminate;
            }
            else if (is_digit(input)) {
                req.http_version_minor = req.http_version_minor * 10 + input - '0';
                return indeterminate;
            }
            else {
                return false;
            }
        case expecting_newline_1:
            _length = 0;
            if (input == '\n') {
                _state = header_line_start;
                return indeterminate;
            }
            else {
                return false;
            }
        case header_line_start:
            if (input == '\r') {
                _state = expecting_newline_3;
                return indeterminate;
            }
            else if (!req.headers.empty() && (input == ' ' || input == '\t')) {
                _state = header_lws;
                return indeterminate;
            }
            else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
                return false;
            }
            else {
                _name.clear();                
                _name.push_back(input);
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
                return false;
            }
            else {
                _state = header_value;
                _value.clear();
                _value.push_back(input);
                return indeterminate;
            }
        case header_name:
            if (input == ':') {
                _state = space_before_header_value;
                return indeterminate;
            }
            else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
                return false;
            }
            else {
                _name.push_back(input);
                return indeterminate;
            }
        case space_before_header_value:
            if (input == ' ') {
                _state = header_value;
                _value.clear();
                return indeterminate;
            }
            else {
                return false;
            }
        case header_value:
            if (input == '\r') {
                req.headers[_name] = _value;
                if (_name == "Content-Length") {
                    istringstream(_value) >> _length;
                }
                _state = expecting_newline_2;
                return indeterminate;
            }
            else if (is_ctl(input)) {
                return false;
            }
            else {
                _value.push_back(input);
                return indeterminate;
            }
        case expecting_newline_2:
            if (input == '\n') {
                _state = header_line_start;
                return indeterminate;
            }
            else {
                return false;
            }
        case expecting_newline_3:
            if (input == '\n') {
                if(_length) {
                    _state = payload;
                    req.payload.clear();
                    return indeterminate;
                }
                else
                    return true;
            }
            else
                return false;
        case payload:
            req.payload.push_back(input);
            if(req.payload.size() < _length)
                return indeterminate;
            else
                return true;
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
