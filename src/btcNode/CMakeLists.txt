
# FIXME: For OS X, need flag for Framework or dylib
IF(DYNAMIC_BITCOIN)
    ADD_DEFINITIONS(-DBTCNODE_LIBRARY)
ELSE()
    ADD_DEFINITIONS(-DBTCNODE_LIBRARY_STATIC)
ENDIF()

SET(LIB_NAME btcNode)

SET(HEADER_PATH ${PROJECT_SOURCE_DIR}/include/${LIB_NAME})
SET(TARGET_H
    ${HEADER_PATH}/main.h
    ${HEADER_PATH}/db.h
    ${HEADER_PATH}/irc.h
    ${HEADER_PATH}/net.h
    ${HEADER_PATH}/Alert.h
    ${HEADER_PATH}/Block.h
    ${HEADER_PATH}/BlockFile.h
    ${HEADER_PATH}/Inventory.h
    ${HEADER_PATH}/Endpoint.h
    ${HEADER_PATH}/EndpointPool.h
    ${HEADER_PATH}/MessageHeader.h
    ${HEADER_PATH}/MessageHandler.h
    ${HEADER_PATH}/MessageParser.h
    ${HEADER_PATH}/Node.h
    ${HEADER_PATH}/Peer.h
    ${HEADER_PATH}/PeerManager.h
    ${BITCOIN_CONFIG_HEADER}
)

#ADD_LIBRARY(${LIB_NAME}
#    ${BITCOIN_USER_DEFINED_DYNAMIC_OR_STATIC}
#    ${LIB_PUBLIC_HEADERS}
SET(TARGET_SRC
    main.cpp
    db.cpp
    irc.cpp
    net.cpp
    Alert.cpp
    Block.cpp
    BlockFile.cpp
    Inventory.cpp
    Endpoint.cpp
    EndpointPool.cpp
    MessageHeader.cpp
    MessageHandler.cpp
    MessageParser.cpp
    Node.cpp
    Peer.cpp
    PeerManager.cpp

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

