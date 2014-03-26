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

#ifndef HTTP_REQUEST_PARSER_H
#define HTTP_REQUEST_PARSER_H

#include <coinHTTP/Export.h>

#include <string>
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>

class Request;

/// Parser for incoming requests.
class COINHTTP_EXPORT RequestParser
{
public:
    /// Construct ready to parse the request method.
    RequestParser();
    
    /// Reset to initial parser state.
    void reset();
    
    /// Parse some data. The tribool return value is true when a complete request
    /// has been parsed, false if the data is invalid, indeterminate when more
    /// data is required. The InputIterator return value indicates how much of the
    /// input has been consumed.
    template <typename InputIterator>
    boost::tuple<boost::tribool, InputIterator> parse(Request& req, InputIterator begin, InputIterator end) {
        while (begin != end) {
            boost::tribool result = consume(req, *begin++);
            if (result || !result)
                return boost::make_tuple(result, begin);
        }
        boost::tribool result = boost::indeterminate;
        return boost::make_tuple(result, begin);
    }
    
private:
    /// Handle the next character of input.
    boost::tribool consume(Request& req, char input);
    
    /// Check if a byte is an HTTP character.
    static bool is_char(int c);
    
    /// Check if a byte is an HTTP control character.
    static bool is_ctl(int c);
    
    /// Check if a byte is defined as an HTTP tspecial character.
    static bool is_tspecial(int c);
    
    /// Check if a byte is a digit.
    static bool is_digit(int c);
    
    /// decode URL
    bool urlDecode(const std::string& in, std::string& out) const;
    
    /// The current state of the parser.
    enum state {
        method_start,
        method,
        uri_start,
        uri,
        http_version_h,
        http_version_t_1,
        http_version_t_2,
        http_version_p,
        http_version_slash,
        http_version_major_start,
        http_version_major,
        http_version_minor_start,
        http_version_minor,
        expecting_newline_1,
        header_line_start,
        header_lws,
        header_name,
        space_before_header_value,
        header_value,
        expecting_newline_2,
        expecting_newline_3,
        payload
    } _state;
    
    std::string _method;
    std::string _uri;
    int _version_major;
    int _version_minor;
    std::string _payload;
    
    std::string _name;
    std::string _value;
    unsigned int _length;
};

#endif // HTTP_REQUEST_PARSER_H
