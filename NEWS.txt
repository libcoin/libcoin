libcoin is a crypto currency library based on the bitcoin/bitcoin "Satoshi" client.

Copenhagen, Denmark - 1st February 2012 Ceptacle announces the release of the first version of the crypto currency library "libcoin" based on the bitcoin/bitcoin "Satoshi" client.

libcoin maintains a version of bitcoind that is 100% compatible drop-in replacement of the bitcoin/bitcoind client: You can use it on the same computer on the same files and you can call it with the same scripts. And you can easily extend it without touching the basic bitcoin source files.

The libcoin/bitcoind client downloads the entire block chain 3.5 times faster than the bitcoin/bitcoind client. This is less than 90 minutes on a modern laptop!

In libcoin, the Satoshi client code has been completely refactored, properly encapsulating classes, removing all globals, moving from threads and mutexes to a pure asynchronous approach. Functionalities have been divided into logical units and libraries, minimizing dependencies for e.g. thin clients.

libcoin is chain agnostic, all chain (bitcoin, testnet, namecoin, litecoin, ...) specific settings are maintained from a single class (Chain) and hence experiments with chain settings, mining, security and digital currencies for research and educational purposes are easily accessible. See the ponzicoin example for how you define your own chain.

The build system of libcoin is based on CMake and supports builds of static and dynamic libraries on Linux, Mac OS X, and Windows.

The libcoin license is LGPL v. 3. This mean that you can use it in open source as well as in commercial projects, but improvements should go back into the libcoin library.
