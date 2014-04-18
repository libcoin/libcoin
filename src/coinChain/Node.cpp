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

#include <coinChain/Node.h>
#include <coinChain/PeerManager.h>
#include <coinChain/VersionFilter.h>
#include <coinChain/EndpointFilter.h>
#include <coinChain/BlockFilter.h>
#include <coinChain/TransactionFilter.h>
#include <coinChain/FilterHandler.h>
#include <coinChain/AlertFilter.h>
#include <coin/Logger.h>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <signal.h>

using namespace std;
using namespace boost;
using namespace asio;

Node::Node(const Chain& chain, std::string dataDir, const string& address, const string& port, ip::tcp::endpoint proxy, unsigned int timeout, const string& irc) :
    _dataDir(dataDir),
    _fileLock(_dataDir == "" ? "" : _dataDir + "/.lock"),
    _io_service(),
    _acceptor(_io_service),
    _blockChain(chain, _dataDir),
    _peerManager(*this),
    _connection_deadline(_io_service),
    _messageHandler(),
    _endpointPool(chain.defaultPort(), _dataDir),
    _chatClient(_io_service, boost::bind(&Node::post_accept_or_connect, this), irc, _endpointPool, chain.ircChannel(), chain.ircChannels(), proxy),
    _proxy(proxy),
    _connection_timeout(timeout),
    _client_name("libcoin"),
    _client_comments(std::vector<std::string>()),
    _client_version(LIBRARY_VERSION) // This is not optimal...
    {
    
    //    _endpointPool.loadEndpoints(dataDir);
    _transactionFilter = filter_ptr(new TransactionFilter(_blockChain));
    _blockFilter = filter_ptr(new BlockFilter(_blockChain));
    _shareFilter = filter_ptr(new ShareFilter(_blockChain));
    
    // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
    ip::tcp::resolver resolver(_io_service);

        if (address.size()) {
            boost::system::error_code ec;
            unsigned short bind_port = (port == "0" ? chain.defaultPort() : lexical_cast<unsigned short>(port));
            do {
                ip::tcp::resolver::query query(address, lexical_cast<string>(bind_port++));
                ip::tcp::endpoint endpoint = *resolver.resolve(query);
                _acceptor.close(ec);
                _acceptor.open(endpoint.protocol());
                _acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
                _acceptor.bind(endpoint, ec);
            } while (ec && port == "0" && bind_port != 0);
            if (ec)
                throw runtime_error("Could not bind to port" + lexical_cast<string>(bind_port-1));
            _acceptor.listen();
        }
    // else nolisten
    
    // ensure we have some addresses to start with (dns seed)
    for (set<string>::const_iterator seed = chain.seeds().begin(); seed != chain.seeds().end(); ++seed) {
        addPeer(*seed);
    }

    // disable chat by blanking out the chat server address
    if (!irc.size()) {
        post_accept_or_connect();
    }

    
    
    // this setup the blockchain to use the database as validation index, and a purged client.
    //    _blockChain.validation_index(BlockChain::MerkleTrie);
    //    _blockChain.purge_depth();
}

void Node::readBlockFile(string path, int fileno) {
    ostringstream os;
    os << path << "blk" << setfill('0') << setw(5) << fileno << ".dat";
    log_info("Processing %s", os.str());
    ifstream ifs(os.str().c_str(), ios::in|ios::binary);
    if (!ifs.is_open())
        return;
    
    Block block;
    Magic magic;
    unsigned int size;
    while (ifs >> binary<Magic>(magic) >> binary<unsigned int>(size) >> block) {
        if (magic != _blockChain.chain().magic())
            throw runtime_error("Trying to read blockfile from wrong chain - magic mismatch");
        if (!_blockChain.haveBlock(block.getHash()) && _blockChain.haveBlock(block.getPrevBlock()))
            _blockChain.append(block);
    }
    readBlockFile(path, fileno+1);
}

void Node::verification(Strictness v) {
    _verification = v;
    update_verification();
}

void Node::update_verification() {
    switch (_verification) {
        case FULL:
            _blockChain.verification_depth(1);
            break;
        case LAST_CHECKPOINT:
            _blockChain.verification_depth(_blockChain.chain().totalBlocksEstimate());
            break;
        case LAZY:
        case MINIMAL:
            _blockChain.verification_depth(_peerManager.getPeerMedianNumBlocks()-_blockChain.chain().maturity(_blockChain.getBestHeight()));
            break;
        case NONE:
            _blockChain.verification_depth(0);
            break;
    }
}

void Node::validation(Strictness v) {
    _validation = v;
    update_validation();
}

void Node::update_validation() {
    switch (_validation) {
        case FULL:
            _blockChain.validation_depth(1);
            break;
        case LAST_CHECKPOINT:
            _blockChain.validation_depth(_blockChain.chain().totalBlocksEstimate());
            break;
        case LAZY:
        case MINIMAL:
            _blockChain.validation_depth(_peerManager.getPeerMedianNumBlocks()-_blockChain.chain().maturity(_blockChain.getBestHeight()));
            break;
        case NONE:
            _blockChain.validation_depth(0);
            break;
    }
}

void Node::persistence(Strictness p) {
    _persistence = p;
    update_persistence();
}

// we set the purge depth, if last check point, just last checkpoint
// if minimal/lazy: never purge more than 100 blocks back.
void Node::update_persistence() {
    _blockChain.lazy_purging(false);
    switch (_persistence) {
        case FULL:
            _blockChain.purge_depth(0);
            break;
        case LAST_CHECKPOINT:
            _blockChain.purge_depth(_blockChain.chain().totalBlocksEstimate());
            break;
        case LAZY:
            _blockChain.lazy_purging(true);
        case MINIMAL: {
            unsigned int network_depth = _peerManager.getPeerMedianNumBlocks()-_blockChain.chain().maturity(_blockChain.getBestHeight());
            unsigned int internal_depth = _blockChain.getBestHeight()-_blockChain.chain().maturity(_blockChain.getBestHeight());
            _blockChain.purge_depth(std::min(network_depth, internal_depth));
            break;
        }
        case NONE:
            break;
    }
}

void Node::run() {
    Logger::label_thread(_blockChain.chain().name());
    // The io_service::run() call will block until all asynchronous operations
    // have finished. While the server is running, there is always at least one
    // asynchronous operation outstanding: the asynchronous accept call waiting
    // for new incoming connections.
    
    //    if(_acceptor.is_open()) start_accept();
    //    start_connect();
    
    // Install filters for the messages. First inserted filters are executed first.
    _messageHandler.installFilter(filter_ptr(new VersionFilter()));
    _messageHandler.installFilter(filter_ptr(new EndpointFilter(_endpointPool)));
    _messageHandler.installFilter(_blockFilter);
    _messageHandler.installFilter(_shareFilter);
    _messageHandler.installFilter(_transactionFilter);
    _messageHandler.installFilter(filter_ptr(new FilterHandler())); // this only output the alert to stdout
    _messageHandler.installFilter(filter_ptr(new AlertFilter(_blockChain.chain().alert_key(), _blockChain.chain().protocol_version(), getFullClientVersion()))); // this only output the alert to stdout
    
    _io_service.run();
}

void Node::setClientVersion(std::string name, std::vector<std::string> comments, int client_version) {
    _client_name = name;
    _client_comments = comments;
    _client_version = client_version;
}

int Node::getClientVersion() const {
    return _client_version;
}
std::string Node::getFullClientVersion() const {
    std::ostringstream ss;
    ss << "/";
    ss << _client_name << ":" << FormatVersion(_client_version);
    if (!_client_comments.empty())
    ss << "(" << boost::algorithm::join(_client_comments, "; ") << ")";
    ss << "/";
    return ss.str();
}

void Node::addPeer(string hostport) {
    // split by the colon
    size_t colon = hostport.find(':');
    string address = hostport.substr(0, colon);
    string port = (colon == string::npos) ? lexical_cast<string>(_blockChain.chain().defaultPort()) : hostport.substr(colon+1);
    ip::tcp::resolver resolver(_io_service);
    ip::tcp::resolver::query query(address, port);
    // iterate over all endpoints returned....
    try {
        for (ip::tcp::resolver::iterator ep = resolver.resolve(query); ep != ip::tcp::resolver::iterator(); ++ep) {
            if (ep->endpoint().address().is_v4())
                addPeer(*ep);
        }
    }
    catch (std::exception& e) {
        log_warn("addPeer failed resolving %s", hostport);
    }
}

void Node::addPeer(boost::asio::ip::tcp::endpoint ep) {
    _endpointPool.addEndpoint(ep);
}

void Node::connectPeer(std::string hostport) {
    // split by the colon
    size_t colon = hostport.find(':');
    string address = hostport.substr(0, colon);
    string port = (colon == string::npos) ? lexical_cast<string>(_blockChain.chain().defaultPort()) : hostport.substr(colon+1);
    ip::tcp::resolver resolver(_io_service);
    ip::tcp::resolver::query query(address, port);
    ip::tcp::endpoint ep = *resolver.resolve(query);
    connectPeer(ep);
}

void Node::connectPeer(boost::asio::ip::tcp::endpoint ep) {
    _connection_list.insert(ep);
}

void Node::updatePeers(const vector<string>& eps) {
    _connection_list.clear();
    for (vector<string>::const_iterator hostport = eps.begin(); hostport != eps.end(); ++hostport) {
        // split by the colon
        size_t colon = hostport->find(':');
        string address = hostport->substr(0, colon);
        string port = (colon == string::npos) ? lexical_cast<string>(_blockChain.chain().defaultPort()) : hostport->substr(colon+1);
        ip::tcp::resolver resolver(_io_service);
        ip::tcp::resolver::query query(address, port);
        ip::tcp::endpoint ep = *resolver.resolve(query);
        
        connectPeer(ep);
    }
}


Endpoint Node::getCandidate(const set<unsigned int>& not_in) {
    Endpoint ep;
    for (endpoints::iterator candidate = _connection_list.begin(); candidate != _connection_list.end(); ++candidate) {
        if(not_in.count(Endpoint(*candidate).getIP()) == 0) {
            ep = *candidate;   
            break;
        }
    }
    return ep;
}

void Node::start_connect() {
    // we connect to 8 peers, but we take one by one and only connect to new peers if we have less than 8 connections.
    set<unsigned int> not_in = _peerManager.getPeerIPList();
    Endpoint ep;
    if(_connection_list.size())
        ep = getCandidate(not_in);
    else
        ep = _endpointPool.getCandidate(not_in, 0);
    if(!ep.isValid())
        return; // this will cause the Node to wait for inbound connections only - alternatively we should add a timer
    // TODO: we should check for validity of the candidate - if not valid we could retry later, give up or wait for a new Peer before we try a new connect. 
    stringstream ss;
    ss << (boost::asio::ip::tcp::endpoint)ep;
    log_debug("Trying connect to: %s", ss.str());
    _new_server.reset(new Peer(_blockChain.chain(), _io_service, _peerManager, _messageHandler, false, _proxy, getFullClientVersion())); // false means outbound
    _new_server->addr = ep;
    // Set a deadline for the connect operation.
    _connection_deadline.expires_from_now(posix_time::milliseconds(_connection_timeout));
    // if using a socks4 proxy - we would here establish he connection to the socks server. 
    if(_proxy)
        _proxy(_new_server->socket()).async_connect(ep, boost::bind(&Node::handle_connect, this, asio::placeholders::error));
    else
        _new_server->socket().async_connect(ep, boost::bind(&Node::handle_connect, this, asio::placeholders::error));
    // start wait for deadline to expire.
    _connection_deadline.async_wait(boost::bind(&Node::check_deadline, this, asio::placeholders::error));
}

void Node::check_deadline(const boost::system::error_code& e) {
    if(!e) {
        // Check whether the deadline has passed. We compare the deadline against
        // the current time.
        if (_connection_deadline.expires_at() <= deadline_timer::traits_type::now()) {
            // The deadline has passed. The socket is closed so that any outstanding
            // asynchronous operations are cancelled.
            if(_new_server) {
                log_debug("Closing socket of: %s", _new_server->addr.toString().c_str());
                _new_server->socket().close();
            }
            
            // There is no longer an active deadline. The expiry is set to positive
            // infinity so that the actor takes no action until a new deadline is set.
            //_connection_deadline.expires_at(posix_time::pos_infin);
        }        
    }
    else if (e != error::operation_aborted) {
        log_warn("Boost deadline timer error in Node: %s", e.message().c_str());
    }
}

void Node::handle_connect(const system::error_code& e) {    
    _connection_deadline.cancel(); // cancel the deadline timer //expires_at(posix_time::pos_infin);
    
    if (!e && _new_server->socket().is_open()) {
        _peerManager.start(_new_server);
    }
    else {
        log_debug("Failed connect: \"%s\" to: %s", e.message().c_str(), _new_server->addr.toString().c_str());
    }
    
    //    _endpointPool.getEndpoint(_new_server->addr.getKey()).setLastTry(GetAdjustedTime());
    _endpointPool.setLastTry(_new_server->addr);
    
    // if we have a proxy error - don't reconnect - wait for some action to be taken...
    
    _new_server.reset();    
    accept_or_connect();
}

void Node::start_accept() {
    _new_client.reset(new Peer(_blockChain.chain(), _io_service, _peerManager, _messageHandler, true, _proxy, getFullClientVersion())); // true means inbound
    _acceptor.async_accept(_new_client->socket(), boost::bind(&Node::handle_accept, this, asio::placeholders::error));
}

void Node::handle_accept(const system::error_code& e) {
    // Check whether the server was stopped by a signal before this completion
    // handler had a chance to run.
    if (!_acceptor.is_open()) {
        return;
    }
    
    if (!e) {
        _peerManager.start(_new_client);
    }

    _new_client.reset(); // this enables us to test if we have pending accepts
    
    accept_or_connect();
}

void Node::accept_or_connect() {
    // only start a connect or accept if we have not one pending already
    if (!_new_client) {
        log_debug("Inbound connections are now: %d", _peerManager.getNumInbound());
        
        if (_peerManager.getNumInbound() < _max_inbound) // start_accept will not be called again before we get a read/write error on a socket
            if(_acceptor.is_open()) start_accept();
    }
        
    if (!_new_server) {
        log_debug("Outbound connections are now: %d", _peerManager.getNumOutbound());
        
        if (_peerManager.getNumOutbound() < _max_outbound) // start_accept will not be called again before we get a read/write error on a socket
            start_connect();         
    }
}

void Node::peer_ready(peer_ptr p) {
    update_persistence();
    update_validation();
    update_verification();
    log_info("PEER: #%d %s %s", (p->inbound())?_peerManager.getNumInbound():_peerManager.getNumOutbound(), p->addr.toString(), (p->inbound())?"inbound":"outbound");
    log_info("\tversion: %d, blocks: %d", p->version(), p->getStartingHeight());
}

void Node::post_accept_or_connect() {
    _io_service.post(boost::bind(&Node::accept_or_connect, this));
}

void Node::post_stop(peer_ptr p) {
    _io_service.post(boost::bind(&PeerManager::stop, &_peerManager, p));
}

void Node::post_ready(peer_ptr p) {
    _io_service.post(boost::bind(&Node::peer_ready, this, p));
}

void Node::handle_stop() {
    // The server is stopped by cancelling all outstanding asynchronous
    // operations. Once all operations have finished the io_service::run() call
    // will exit.
    _acceptor.close();
    _peerManager.stop_all();
    _io_service.stop();
}

void Node::post(const Transaction& tx, size_t n)
{ 
    Peers peers = _peerManager.getAllPeers();
    Peers some;
    size_t s = peers.size();
    if (s == 0) {
        log_warn("Cannot post txn: %s as no peers are available, relying on a later repost", tx.getHash().toString());
        return;
    }
    if (n != 0 && n < s) {
        for(unsigned int i = 0; i < n; i++) {
            size_t r = GetRand(s);
            Peers::const_iterator p = peers.begin();
            advance(p, r);
            some.insert(*p);
        }
        _io_service.dispatch(boost::bind(&TransactionFilter::process, static_cast<TransactionFilter*>(_transactionFilter.get()), tx, some));
    }
    else
        _io_service.dispatch(boost::bind(&TransactionFilter::process, static_cast<TransactionFilter*>(_transactionFilter.get()), tx, peers));
}

int Node::peerPenetration(const uint256 hash) const {
    // create Inv from hash
    Inventory inv(MSG_TX, hash);
    size_t count = 0;
    Peers peers = _peerManager.getAllPeers();
    for(Peers::iterator p = peers.begin(); p != peers.end(); ++p)
        if ((*p)->setInventoryKnown.count(inv)) count++;
    return count;
}

