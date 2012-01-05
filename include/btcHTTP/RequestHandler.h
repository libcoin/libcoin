
#ifndef HTTP_REQUEST_HANDLER_H
#define HTTP_REQUEST_HANDLER_H

#include "btcHTTP/Method.h"

#include <string>

struct Reply;
struct Request;

/// The common handler for all incoming requests.
class RequestHandler : private boost::noncopyable
{
public:
    /// Construct with a directory containing files to be served.
    explicit RequestHandler(const std::string& doc_root);
    
    /// Register an application handler, e.g. for RPC or CGI
    void registerMethod(method_ptr method);
    
    /// Remove an application handler, e.g. for RPC or CGI
    void unregisterMethod(const std::string name);
    
    /// Handle a GET request and produce a reply.
    void handleGET(const Request& req, Reply& rep);
    
    /// Handle a POST request and produce a reply.
    void handlePOST(const Request& req, Reply& rep);
    
    /// Clear the document cache.
    void clearDocCache();
    
    /// Get document cache statistics.
    std::string getDocCacheStats(int level = 0);
    
private:
    /// The directory containing the files to be served.
    std::string _doc_root;
    
    /// The document cache. Not for huge data amounts!
    typedef std::map<std::string, std::string> DocCache;
    DocCache _doc_cache;
    
    Methods _methods;
    
    /// Perform URL-decoding on a string. Returns false if the encoding was
    /// invalid.
    static bool urlDecode(const std::string& in, std::string& out);
};

#endif // HTTP_REQUEST_HANDLER_HPP
