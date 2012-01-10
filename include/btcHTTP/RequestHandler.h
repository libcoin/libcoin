
#ifndef HTTP_REQUEST_HANDLER_H
#define HTTP_REQUEST_HANDLER_H

#include "btcHTTP/Method.h"

#include <string>

struct Reply;
struct Request;

class Auth
{
public:
    Auth() {}
    Auth(std::string username, std::string password) {
        _base64auth = encode64(username + ":" + password);
    }
    
    bool isNone() { return _base64auth.size() == 0; }
    void setNone() { _base64auth.clear(); }

    bool valid(std::string auth) { return _base64auth == auth; }
    
    std::string username();
    std::string password();
    
private:
    std::string encode64(std::string s);
    std::string decode64(std::string s);
    std::string _base64auth;
};

typedef std::map<std::string, Auth> Auths;

/// The common handler for all incoming requests.
class RequestHandler : private boost::noncopyable
{
public:
    /// Construct with a directory containing files to be served.
    explicit RequestHandler(const std::string& doc_root);
    
    /// Register an application handler, e.g. for RPC or CGI
    void registerMethod(method_ptr method) {
        Auth none;
        registerMethod(method, none);
    }
    
    /// Register an application handler, e.g. for RPC or CGI
    void registerMethod(method_ptr method, Auth auth);
    
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
    
    Auths _auths;
    
    /// Perform URL-decoding on a string. Returns false if the encoding was
    /// invalid.
    static bool urlDecode(const std::string& in, std::string& out);
};

#endif // HTTP_REQUEST_HANDLER_HPP
