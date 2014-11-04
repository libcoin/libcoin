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
#include <coinChain/Alert.h>
#include <coin/Logger.h>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

#include <algorithm>

#include <signal.h>
#include <cstdlib>

using namespace std;
using namespace boost;
using namespace asio;

class BlockListener : public BlockFilter::Listener {
public:
    BlockListener(Node& node) : BlockFilter::Listener(), _node(node) {}
    
    virtual void operator()(const Block& block) {
        // locate a block that is 6 blocks behind - this is the right block to look in
        _node.handle_block();
    }
    
    virtual ~BlockListener() {}
    
private:
    Node& _node;
};


int NodeNotifier::operator()(std::string message) {
    return _node.notify(message);
}

Node::Node(const Chain& chain, std::string dataDir, const string& address, const string& port, ip::tcp::endpoint proxy, unsigned int timeout, const string& irc, string notify) :
    _dataDir(dataDir),
    _fileLock(_dataDir == "" ? "" : _dataDir + "/.lock"),
    _notifier(*this),
    _notify(notify),
    _io_service(),
    _acceptor(_io_service),
    _blockChain(chain, _dataDir),
    _peerManager(*this),
    _messageHandler(),
    _endpointPool(chain.defaultPort(), _dataDir),
    _chatClient(_io_service, boost::bind(&Node::post_connect, this), irc, _endpointPool, chain.ircChannel(), chain.ircChannels(), proxy),
    _proxy(proxy),
    _connection_timeout(timeout),
    _client_name("libcoin"),
    _client_comments(std::vector<std::string>()),
    _client_version(LIBRARY_VERSION) // This is not optimal...
    {
    
    //    _endpointPool.loadEndpoints(dataDir);
    _transactionFilter = filter_ptr(new TransactionFilter(_notifier, _blockChain));
    _blockFilter = filter_ptr(new BlockFilter(_notifier, _blockChain));
    _shareFilter = filter_ptr(new ShareFilter(_notifier, _blockChain));
    _alertFilter = filter_ptr(new AlertFilter(_notifier, _blockChain.chain().alert_key(), _blockChain.chain().protocol_version(), getFullClientVersion()));
    
        subscribe(BlockFilter::listener_ptr(new BlockListener(*this)));

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
            start_accept();
        }
    // else nolisten
    
    // ensure we have some addresses to start with (dns seed)
    for (set<string>::const_iterator seed = chain.seeds().begin(); seed != chain.seeds().end(); ++seed) {
        addPeer(*seed);
    }

    // disable chat by blanking out the chat server address
    if (!irc.size()) {
        post_connect();
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

int Node::notify(std::string message) {
    if (_notify.size() && message.size()) {
        std::string cl = "echo \"" + message + "\"|" + _notify;
        return std::system(cl.c_str());
    }
    return 0;
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
            _blockChain.purge_depth(_blockChain.getBestHeight()-_blockChain.chain().maturity(_blockChain.getBestHeight()));
//            unsigned int network_depth = _peerManager.getPeerMedianNumBlocks()-_blockChain.chain().maturity(_blockChain.getBestHeight());
//            unsigned int internal_depth = _blockChain.getBestHeight()-2*_blockChain.chain().maturity(_blockChain.getBestHeight());
//            _blockChain.purge_depth(std::min(network_depth, internal_depth));
//            _blockChain.purge_depth(internal_depth);
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
    
    // Install filters for the messages. First inserted filters are executed first.
    _messageHandler.installFilter(filter_ptr(new VersionFilter(_notifier)));
    _messageHandler.installFilter(filter_ptr(new EndpointFilter(_notifier, _endpointPool)));
    _messageHandler.installFilter(_blockFilter);
    _messageHandler.installFilter(_shareFilter);
    _messageHandler.installFilter(_transactionFilter);
    _messageHandler.installFilter(filter_ptr(new FilterHandler(_notifier))); // this only output the alert to stdout
    _messageHandler.installFilter(filter_ptr(_alertFilter)); // this only output the alert to stdout
    
    handle_block(); // to start the async handling of blocks
    
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
    endpoints candidates;
    for (endpoints::const_iterator candidate = _connection_list.begin(); candidate != _connection_list.end(); ++candidate) {
        if(not_in.count(Endpoint(*candidate).getIP()) == 0) {
            candidates.insert(*candidate);
        }
    }
    endpoints::iterator candidate = candidates.begin();
    if (candidate == candidates.end())
        return Endpoint();

    advance(candidate, rand()%candidates.size());
    return *candidate;
}

void Node::start_connect() {
    while ((_connection_list.empty() && _peerManager.getNumOutbound(true) < _max_outbound) || _peerManager.getNumOutbound(true) <_connection_list.size()) {
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
        boost::shared_ptr<Peer> peer(new Peer(_blockChain.chain(), _io_service, _peerManager, _messageHandler, false, _proxy, getFullClientVersion())); // false means outbound
        peer->connect(ep, _proxy, _connection_timeout);
        _endpointPool.setLastTry(peer->endpoint());
    }
}


void Node::handle_block() {
    int height = _blockChain.getBestHeight();
    int median = _peerManager.getPeerMedianNumBlocks();
    
    // if we are up to date invoke
    if (height >= median) {
        _io_service.post(boost::bind(&Node::handle_async_block, this));
    }
}

void Node::handle_async_block() {
    int count = _blockChain.getBestHeight()+1;
    
    bool done = true;
    for (vector<AsyncListener>::const_iterator l = _async_listeners.begin(); l != _async_listeners.end(); ++l) {
        done &= (*l)(count);
    }
    
    if (!done)
        handle_block();
}

void Node::start_accept() {
    if(_acceptor.is_open()) {
        _new_client.reset(new Peer(_blockChain.chain(), _io_service, _peerManager, _messageHandler, true, _proxy, getFullClientVersion())); // true means inbound
        _acceptor.async_accept(_new_client->socket(), boost::bind(&Node::handle_accept, this, asio::placeholders::error));
    }
}

void Node::handle_accept(const system::error_code& e) {

    if (!e) {
        if (_peerManager.getNumInbound() < _max_inbound) { // if we have room for more connections start it, otherwise drop it
            _peerManager.manage(_new_client);
            _new_client->handle_connect(e);
        }
    }

    start_accept();
}

void Node::post_connect() {
    _io_service.post(boost::bind(&Node::start_connect, this));
}

void Node::handle_stop() {
    // The server is stopped by cancelling all outstanding asynchronous
    // operations. Once all operations have finished the io_service::run() call
    // will exit.
    _acceptor.close();
    _peerManager.stop_all();
    _io_service.stop();
}

void Node::post(const Transaction& tx, size_t n) {
    Peers peers = _peerManager.getAllPeers();
    Peers some;
    size_t s = peers.size();
    log_info("Posting %s to %d peers:", tx.getHash().toString(), s);
    for (Peers::const_iterator p = peers.begin(); p != peers.end(); ++p)
        log_info("\t%s", p->get()->toString());
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

void Node::post(string command, string message) {
    MessageHeader hdr(_blockChain.chain(), command, message);
    Message msg;
    msg.header() = hdr;
    msg.payload() = message;
    
    Peer* origin = _peerManager.getAllPeers().begin()->get();
    
    _messageHandler.handleMessage(origin, msg);
}


int Node::peerPenetration(const uint256 hash) const {
    // create Inv from hash
    Inventory inv(MSG_TX, hash);
    size_t count = 0;
    Peers peers = _peerManager.getAllPeers();
    for(Peers::iterator p = peers.begin(); p != peers.end(); ++p)
        if ((*p)->known(inv)) count++;
    return count;
}

