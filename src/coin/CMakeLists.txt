
# FIXME: For OS X, need flag for Framework or dylib
IF(DYNAMIC_LIBCOIN)
    ADD_DEFINITIONS(-DCOIN_LIBRARY)
ELSE()
    ADD_DEFINITIONS(-DCOIN_LIBRARY_STATIC)
ENDIF()

SET(LIB_NAME coin)

SET(HEADER_PATH ${PROJECT_SOURCE_DIR}/include/${LIB_NAME})
SET(TARGET_H
    ${HEADER_PATH}/serialize.h
    ${HEADER_PATH}/uint256.h
    ${HEADER_PATH}/util.h
    
    ${HEADER_PATH}/Address.h
    ${HEADER_PATH}/AuxPow.h
    ${HEADER_PATH}/BigNum.h
    ${HEADER_PATH}/Block.h
    ${HEADER_PATH}/BlockHeader.h
    ${HEADER_PATH}/Currency.h
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/ExtendedKey.h
    ${HEADER_PATH}/Key.h
    ${HEADER_PATH}/KeyStore.h
    ${HEADER_PATH}/Logger.h
    ${HEADER_PATH}/MerkleTx.h
    ${HEADER_PATH}/NameOperation.h
    ${HEADER_PATH}/Script.h
    ${HEADER_PATH}/Serialization.h
    ${HEADER_PATH}/Transaction.h
    ${HEADER_PATH}/Version.h
    ${LIBCOIN_CONFIG_HEADER}
)

#ADD_LIBRARY(${LIB_NAME}
#    ${LIBCOIN_USER_DEFINED_DYNAMIC_OR_STATIC}
#    ${LIB_PUBLIC_HEADERS}
SET(TARGET_SRC
    scrypt.c
    util.cpp
    
    Address.cpp
    AuxPow.cpp
    Block.cpp
    BlockHeader.cpp
    Currency.cpp
    ExtendedKey.cpp
    Key.cpp
    KeyStore.cpp
    Logger.cpp
    MerkleTx.cpp
    NameOperation.cpp
    Transaction.cpp
    Script.cpp
    Serialization.cpp
    ${LIBCOIN_VERSIONINFO_RC}
)
SET(TARGET_LIBRARIES )


IF(DYNAMIC_LIBCOIN)
SET(TARGET_EXTERNAL_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}    
    ${MATH_LIBRARY} 
    ${OPENSSL_LIBRARIES} 
    ${Boost_LIBRARIES} 
    ${DL_LIBRARY}
)
ENDIF()

SETUP_LIBRARY(${LIB_NAME})

