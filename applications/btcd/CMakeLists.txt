
SOURCE_GROUP("Document Root\\wallet" FILES
www/wallet/index.html
)

SOURCE_GROUP("Document Root\\donate" FILES
www/donate/index.html
)

SOURCE_GROUP("Document Root\\js" FILES
www/js/btc.js_
www/js/jquery.js_
)

SOURCE_GROUP("Document Root\\js\\btc" FILES
www/js/btc/base58.js
www/js/btc/ecdsa.js
www/js/btc/eckey.js
www/js/btc/address.js
www/js/btc/util.js
www/js/btc/opcode.js
www/js/btc/script.js
www/js/btc/tx.js
www/js/btc/asset.js
)

SOURCE_GROUP("Document Root\\js\\btc\\jsbn" FILES
www/js/btc/jsbn/jsbn.js
www/js/btc/jsbn/jsbn2.js
www/js/btc/jsbn/ec.js
www/js/btc/jsbn/sec.js
www/js/btc/jsbn/prng4.js
www/js/btc/jsbn/rng.js
)

SOURCE_GROUP("Document Root\\js\\btc\\crypto-js" FILES
www/js/btc/crypto-js/crypto.js
www/js/btc/crypto-js/sha256.js
www/js/btc/crypto-js/ripemd160.js
)

SET(TARGET_SRC
proxy.cpp
proxy.h
btcd.cpp
www/wallet/index.html
www/donate/index.html
www/js/btc.js_
www/js/jquery.js_
www/js/btc/base58.js
www/js/btc/ecdsa.js
www/js/btc/eckey.js
www/js/btc/address.js
www/js/btc/util.js
www/js/btc/opcode.js
www/js/btc/script.js
www/js/btc/tx.js
www/js/btc/asset.js
www/js/btc/jsbn/jsbn.js
www/js/btc/jsbn/jsbn2.js
www/js/btc/jsbn/ec.js
www/js/btc/jsbn/sec.js
www/js/btc/jsbn/prng4.js
www/js/btc/jsbn/rng.js
www/js/btc/crypto-js/crypto.js
www/js/btc/crypto-js/sha256.js
www/js/btc/crypto-js/ripemd160.js
)

SETUP_APPLICATION(btcd)
