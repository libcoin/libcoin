libcoin supports building of static and dynamic libraries on:
* Linux
* Mac OS X
* Windows (experimental)
Further, the reference application, libcoind, is supported along with a number of representative examples.

To build libcoin on unix flavor systems, you need to install development versions of Berkeley DB (C++),
Boost and OpenSSL and SQLite3. (Please note that on some systems OpenSSL does not contain ECDSA, if so,
please install it yourselves). After installation of the necessary dependencies building follows the standard scheme:
* ./configure
* make
* sudo make install
If you would like to tweak the installation, run the configure part manually by calling:
* cmake . or ccmake .
with the preferred generator. E.g. on Mac OS X building with Xcode can be accomplished by:
* cmake -GXcode .
Then open libcore.xcodeproj using Xcode and build libcoin from here.

Cheat sheet for Ubuntu:
* sudo apt-get install git cmake build-essential
* sudo apt-get install libboost-all-dev
* sudo apt-get install libssl-dev
* sudo apt-get install libsqlite3-dev
* sudo apt-get install libdb++-dev
* git clone https://github.com/libcoin/libcoin.git
* cd libcoin
* ./configure
* make
* sudo make install

On windows separate building instructions can be found online:
http://github.com/ceptacle/libcoin/wiki/Windows-build
