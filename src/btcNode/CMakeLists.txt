
# FIXME: For OS X, need flag for Framework or dylib
IF(DYNAMIC_BITCOIN)
    ADD_DEFINITIONS(-DBTCNODE_LIBRARY)
ELSE()
    ADD_DEFINITIONS(-DBTCNODE_LIBRARY_STATIC)
ENDIF()

SET(LIB_NAME btcNode)

SET(HEADER_PATH ${PROJECT_SOURCE_DIR}/include/${LIB_NAME})
SET(TARGET_H
    ${HEADER_PATH}/db.h
    ${HEADER_PATH}/Alert.h
    ${HEADER_PATH}/AlertFilter.h
    ${HEADER_PATH}/Block.h
    ${HEADER_PATH}/BlockIndex.h
    ${HEADER_PATH}/BlockFile.h
    ${HEADER_PATH}/BlockChain.h
    ${HEADER_PATH}/BlockFilter.h
    ${HEADER_PATH}/ChatClient.h
    ${HEADER_PATH}/Endpoint.h
    ${HEADER_PATH}/EndpointPool.h
    ${HEADER_PATH}/EndpointFilter.h
    ${HEADER_PATH}/Filter.h
    ${HEADER_PATH}/Inventory.h    
    ${HEADER_PATH}/MessageHeader.h
    ${HEADER_PATH}/MessageHandler.h
    ${HEADER_PATH}/MessageParser.h
    ${HEADER_PATH}/Node.h
    ${HEADER_PATH}/Peer.h
    ${HEADER_PATH}/PeerManager.h
    ${HEADER_PATH}/TransactionFilter.h
    ${HEADER_PATH}/VersionFilter.h
    ${HEADER_PATH}/WebClient.h
    ${BITCOIN_CONFIG_HEADER}
)

#ADD_LIBRARY(${LIB_NAME}
#    ${BITCOIN_USER_DEFINED_DYNAMIC_OR_STATIC}
#    ${LIB_PUBLIC_HEADERS}
SET(TARGET_SRC
    db.cpp
    Alert.cpp
    AlertFilter.cpp
    Block.cpp
    BlockFilter.cpp
    BlockIndex.cpp
    BlockFile.cpp
    BlockChain.cpp
    ChatClient.cpp
    Endpoint.cpp
    EndpointPool.cpp
    EndpointFilter.cpp
    Inventory.cpp
    MessageHeader.cpp
    MessageHandler.cpp
    MessageParser.cpp
    Node.cpp
    Peer.cpp
    PeerManager.cpp
    TransactionFilter.cpp
    VersionFilter.cpp
    WebClient.cpp

    ${BITCOIN_VERSIONINFO_RC}
)
SET(TARGET_LIBRARIES btc)

SET(TARGET_EXTERNAL_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}    
    ${MATH_LIBRARY} 
    ${SSL_LIBRARY} 
    ${CRYPTO_LIBRARY} 
    ${BDB_LIBRARY} 
    ${BOOST_LIBRARIES} 
    ${DL_LIBRARY}
)

SETUP_LIBRARY(${LIB_NAME})

