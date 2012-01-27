
#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include "coinHTTP/Header.h"

/// A request received from a client.
struct Request {
    std::string method;
    std::string uri;
    int http_version_major;
    int http_version_minor;
    Headers headers;
    std::string payload;
};

#endif // HTTP_REQUEST_H
