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

#ifndef HTTP_REQUEST_HANDLER_H
#define HTTP_REQUEST_HANDLER_H

#include <coinHTTP/Export.h>
#include <coinHTTP/Method.h>
#include <coinHTTP/Header.h>

#include <string>

struct Reply;
struct Request;

class COINHTTP_EXPORT Auth
{
public:
    Auth() {}
    Auth(std::string username, std::string password) {
        _base64auth = encode64(username + ":" + password);
    }
    
    bool isNone() { return _base64auth.size() == 0; }
    void setNone() { _base64auth.clear(); }

    bool valid(std::string auth) {
        if(isNone())
            return true;
        else if (_base64auth == encode64(":"))
            return false;
        else
            return _base64auth == auth;
    }
    
    std::string username();
    std::string password();
    
    Headers headers() {
        Headers h;
        if (isNone() || _base64auth == encode64(":"))
            return h;
        h["Authorization"] = "Basic " + _base64auth;
        return h;
    }
    
    static std::string encode64(std::string s);
    static std::string decode64(std::string s);
private:
    std::string _base64auth;
};

typedef std::map<std::string, Auth> Auths;

/// The common handler for all incoming requests.
class COINHTTP_EXPORT RequestHandler : private boost::noncopyable
{
public:
    /// Construct with a directory containing files to be served.
    explicit RequestHandler(const std::string& doc_root);
    
	/// Set the doc root after initialization.
	void setDocRoot(const std::string& doc_root) { _doc_root = doc_root; }

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
