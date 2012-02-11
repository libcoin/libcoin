
# FIXME: For OS X, need flag for Framework or dylib
IF(DYNAMIC_LIBCOIN)
    ADD_DEFINITIONS(-DCOINCHAIN_LIBRARY)
ELSE()
    ADD_DEFINITIONS(-DCOINCHAIN_LIBRARY_STATIC)
ENDIF()

SET(LIB_NAME coinChain)

SET(HEADER_PATH ${PROJECT_SOURCE_DIR}/include/${LIB_NAME})
SET(TARGET_H
    ${HEADER_PATH}/db.h
    ${HEADER_PATH}/Alert.h
    ${HEADER_PATH}/AlertFilter.h
    ${HEADER_PATH}/BlockIndex.h
    ${HEADER_PATH}/BlockFile.h
    ${HEADER_PATH}/BlockChain.h
    ${HEADER_PATH}/BlockFilter.h
    ${HEADER_PATH}/Chain.h
    ${HEADER_PATH}/ChatClient.h
    ${HEADER_PATH}/Endpoint.h
    ${HEADER_PATH}/EndpointPool.h
    ${HEADER_PATH}/EndpointFilter.h
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/Filter.h
    ${HEADER_PATH}/Inventory.h    
    ${HEADER_PATH}/MessageHeader.h
    ${HEADER_PATH}/MessageHandler.h
    ${HEADER_PATH}/MessageParser.h
    ${HEADER_PATH}/Node.h
    ${HEADER_PATH}/NodeRPC.h
    ${HEADER_PATH}/Peer.h
    ${HEADER_PATH}/PeerManager.h
    ${HEADER_PATH}/TransactionFilter.h
    ${HEADER_PATH}/VersionFilter.h
    ${LIBCOIN_CONFIG_HEADER}
)

#ADD_LIBRARY(${LIB_NAME}
#    ${LIBCOIN_USER_DEFINED_DYNAMIC_OR_STATIC}
#    ${LIB_PUBLIC_HEADERS}
SET(TARGET_SRC
    db.cpp
    Alert.cpp
    AlertFilter.cpp
    BlockFilter.cpp
    BlockIndex.cpp
    BlockFile.cpp
    BlockChain.cpp
    Chain.cpp
    ChatClient.cpp
    Endpoint.cpp
    EndpointPool.cpp
    EndpointFilter.cpp
    Inventory.cpp
    MessageHeader.cpp
    MessageHandler.cpp
    MessageParser.cpp
    Node.cpp
    NodeRPC.cpp
    Peer.cpp
    PeerManager.cpp
    TransactionFilter.cpp
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
    ${BDB_LIBRARY} 
    ${DL_LIBRARY}
)
ENDIF()

SETUP_LIBRARY(${LIB_NAME})

