
#include <coinChain/Configuration.h>
#include <boost/asio/ip/address_v4.hpp>

using namespace std;
using namespace boost;
using namespace boost::program_options;

Node::Strictness strictness(std::string s) {
    if (s == "NONE")
        return Node::NONE;
    if (s == "MINIMAL")
        return Node::MINIMAL;
    if (s == "CHECKPOINT")
        return Node::LAST_CHECKPOINT;
    if (s == "LAZY")
        return Node::LAZY;
    if (s == "FULL")
        return Node::FULL;
    throw runtime_error("Cannot parse strictness parameter: " + s);
}

Configuration::Configuration(int argc, char* argv[], const options_description& extra) {
    string prog_name = argv[0];
    size_t slash = prog_name.rfind('/');
    if (slash != string::npos)
        prog_name = prog_name.substr(slash+1);
    
    _config_file = prog_name + string(".conf");
    
    // Commandline options
    options_description generic("Generic options");
    generic.add_options()
    ("help,?", "Show help messages")
    ("version,v", "print version string")
    ("conf,c", value<string>(&_config_file)->default_value(_config_file), "Specify configuration file")
    ("datadir", value<string>(&_data_dir), "Specify non default data directory")
    ("bitcoin", "Run as a bitcoin client")
    ("testnet3", "Use the test network")
    ("litecoin", "Run as a litecoin client")
    ("namecoin", "Run as a namecoin client")
    ("dogecoin", "Run as a namecoin client")
    ("ripple", "Run as a ripple client")
    ;
    
    string verification, validation, persistance;
    
    options_description config("Config options");
    config.add_options()
    ("verification", value<string>(&verification)->default_value("MINIMAL"), "Specify the signuture verificatin depth: NONE, MINIMAL: last 100 blocks, CHECKPOINT: last checkpoint, FULL")
    ("validation", value<string>(&validation)->default_value("NONE"), "Specify the depth from which MerkleTrie validation hashes are calculated: NONE, MINIMAL: last 100 blocks, CHECKPOINT: last checkpoint, FULL")
    ("persistence", value<string>(&persistance)->default_value("LAZY"), "Specify the depth of stored blocks: NONE, MINIMAL: last 100 blocks, LAZY: like MINIMAL, but only purge on restart, CHECKPOINT: last checkpoint, FULL")
    ("searchable", value<bool>(&_searchable)->default_value(true), "Enable indexing of addresses/scripts")
    ("pid", value<string>(), "Specify pid file (default: bitcoind.pid)")
    ("nolisten", "Don't accept connections from outside")
    ("portmap", value<bool>(&_portmap)->default_value(true), "Use IGD-UPnP or NATPMP to map the listening port")
    ("upnp", value<bool>(&_portmap), "Use UPnP to map the listening port - deprecated, use portmap")
    ("proxy", value<string>(&_proxy), "Connect through socks4 proxy")
    ("timeout", value<unsigned int>(&_timeout)->default_value(5000), "Specify connection timeout (in milliseconds)")
    ("port", value<unsigned short>(&_port)->default_value(0), "Listen on specified port for the p2p protocol")
    ("rpcuser", value<string>(&_rpc_user), "Username for JSON-RPC connections")
    ("rpcpassword", value<string>(&_rpc_pass), "Password for JSON-RPC connections")
    ("rpcport", value<unsigned short>(&_rpc_port)->default_value(8332), "Listen for JSON-RPC connections on <arg>")
    ("rpcallowip", value<string>(&_rpc_bind)->default_value(asio::ip::address_v4::loopback().to_string()), "Allow JSON-RPC connections from specified IP address")
    ("rpcconnect", value<string>(&_rpc_connect)->default_value(asio::ip::address_v4::loopback().to_string()), "Send commands to node running on <arg>")
    ("keypool", value<unsigned short>(), "Set key pool size to <arg>")
    ("rescan", "Rescan the block chain for missing wallet transactions")
    ("gen", value<bool>(&_gen)->default_value(false), "Generate coins")
    ("rpcssl", value<bool>(&_ssl)->default_value(false), "Use OpenSSL (https) for JSON-RPC connections")
    ("rpcsslcertificatechainfile", value<string>(&_certchain)->default_value("server.cert"), "Server certificate file")
    ("rpcsslprivatekeyfile", value<string>(&_privkey)->default_value("server.pem"), "Server private key")
    ("debug", "Set logging output to debug")
    ("trace", "Set logging output to trace")
    ("log", value<string>(&_log_file)->default_value("debug.log"), "Logfile name - if starting with / absolute path is assumed, otherwise relative to data directory, choose '-' for logging to stderr")
    ;
    
    options_description hidden("Hidden options");
    hidden.add_options()("params", value<strings>(&_params), "Run JSON RPC command");
    
    options_description cmdline_options;
    cmdline_options.add(generic).add(config).add(hidden).add(extra);
    
    options_description config_file_options;
    config_file_options.add(config).add(extra);
    
    _visible.add(generic).add(config).add(extra);
    
    positional_options_description pos;
    pos.add("params", -1);
    
    // parse the command line
    variables_map args;
    store(command_line_parser(argc, argv).options(cmdline_options).positional(pos).run(), args);
    notify(args);
    
    if(args.count("testnet3") || prog_name.find("testnet3") != string::npos)
        _chain = &testnet3;
    else if(args.count("litecoin") || prog_name.find("litecoin") != string::npos)
        _chain = &litecoin;
    else if(args.count("namecoin") || prog_name.find("namecoin") != string::npos)
        _chain = &namecoin;
    else if(args.count("dogecoin") || prog_name.find("dogecoin") != string::npos)
        _chain = &dogecoin;
    else if(args.count("ripple") || prog_name.find("ripped") != string::npos)
        _chain = &ripplecredits;
    else
        _chain = &bitcoin;
    
    if(!args.count("datadir"))
        _data_dir = default_data_dir(_chain->name());
    else if (_data_dir[0] != '/')  // check that the datadir is absolute
        _data_dir = default_data_dir(_data_dir);
    
    // if present, parse the config file - if no data dir is specified we always assume bitcoin chain at this stage
    string config_path = _data_dir + "/" + _config_file;
    ifstream ifs(config_path.c_str());
    if (!ifs)
        ifs.open((_data_dir + "/libcoin.conf").c_str());
    if(ifs) {
        store(parse_config_file(ifs, config_file_options, true), args);
        notify(args);
    }
    
    if (_log_file.size() && _log_file[0] != '-') {
        if (_log_file.size() && _log_file[0] != '/')
            _log_file = _data_dir + "/" + _log_file;
        
        _olog.open((_log_file).c_str(), std::ios_base::out|std::ios_base::app);
    }
    
    Logger::Level ll = Logger::info;
    if (args.count("trace"))
        ll = Logger::trace;
    else if (args.count("debug"))
        ll = Logger::debug;
    
    if (_olog.is_open())
        Logger::instantiate(_olog, ll);
    else
        Logger::instantiate(cerr, ll);
    
    Logger::label_thread("main");
    
    if (args.count("params")) {
        _method = _params[0];
        _params.erase(_params.begin());
    }
    
    _listen = args.count("nolisten") ? "" : "0.0.0.0";
    _verification = strictness(verification);
    _validation = strictness(validation);
    _persistance = strictness(persistance);
}

