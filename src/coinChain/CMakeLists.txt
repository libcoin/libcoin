
# FIXME: For OS X, need flag for Framework or dylib
IF(DYNAMIC_LIBCOIN)
    ADD_DEFINITIONS(-DCOINCHAIN_LIBRARY)
ELSE()
    ADD_DEFINITIONS(-DCOINCHAIN_LIBRARY_STATIC)
ENDIF()

SET(LIB_NAME coinChain)

SET(HEADER_PATH ${PROJECT_SOURCE_DIR}/include/${LIB_NAME})
SET(TARGET_H
    ${HEADER_PATH}/Alert.h
    ${HEADER_PATH}/AlertFilter.h
    ${HEADER_PATH}/BlockChain.h
    ${HEADER_PATH}/BlockFilter.h
    ${HEADER_PATH}/BloomFilter.h
    ${HEADER_PATH}/BlockTree.h
    ${HEADER_PATH}/Chain.h
    ${HEADER_PATH}/ChatClient.h
    ${HEADER_PATH}/Claims.h
    ${HEADER_PATH}/Configuration.h
    ${HEADER_PATH}/Database.h
    ${HEADER_PATH}/Endpoint.h
    ${HEADER_PATH}/EndpointPool.h
    ${HEADER_PATH}/EndpointFilter.h
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/Filter.h
    ${HEADER_PATH}/FilterHandler.h
    ${HEADER_PATH}/Inventory.h    
    ${HEADER_PATH}/MerkleBlock.h
    ${HEADER_PATH}/MerkleTrie.h
    ${HEADER_PATH}/MessageHeader.h
    ${HEADER_PATH}/MessageHandler.h
    ${HEADER_PATH}/MessageParser.h
    ${HEADER_PATH}/Node.h
    ${HEADER_PATH}/NodeRPC.h
    ${HEADER_PATH}/Peer.h
    ${HEADER_PATH}/PeerManager.h
    ${HEADER_PATH}/Proxy.h
    ${HEADER_PATH}/Referenced.h
    ${HEADER_PATH}/Spendables.h
    ${HEADER_PATH}/ref_ptr.h
    ${HEADER_PATH}/TransactionFilter.h
    ${HEADER_PATH}/Verifier.h
    ${HEADER_PATH}/Version.h
    ${HEADER_PATH}/VersionFilter.h
    ${LIBCOIN_CONFIG_HEADER}
)

#ADD_LIBRARY(${LIB_NAME}
#    ${LIBCOIN_USER_DEFINED_DYNAMIC_OR_STATIC}
#    ${LIB_PUBLIC_HEADERS}
SET(TARGET_SRC
    Alert.cpp
    AlertFilter.cpp
    BlockFilter.cpp
    BloomFilter.cpp
    BlockChain.cpp
    BlockTree.cpp
    Chain.cpp
    ChatClient.cpp
    Claims.cpp
    Configuration.cpp
    Database.cpp
    Endpoint.cpp
    EndpointPool.cpp
    EndpointFilter.cpp
    FilterHandler.cpp
    Inventory.cpp
    MerkleBlock.cpp
    MerkleTrie.cpp
    MessageHeader.cpp
    MessageHandler.cpp
    MessageParser.cpp
    Node.cpp
    NodeRPC.cpp
    Peer.cpp
    PeerManager.cpp
    Proxy.cpp
    TransactionFilter.cpp
    Verifier.cpp
    Version.cpp
    VersionFilter.cpp

    ${LIBCOIN_VERSIONINFO_RC}
)
SET(TARGET_LIBRARIES coin coinHTTP)

IF(DYNAMIC_LIBCOIN)
SET(TARGET_EXTERNAL_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}    
    ${MATH_LIBRARY} 
    ${OPENSSL_LIBRARIES} 
    ${Boost_LIBRARIES} 
    ${SQLITE3_LIBRARIES} 
    ${DL_LIBRARY}
)
ENDIF()

SETUP_LIBRARY(${LIB_NAME})

