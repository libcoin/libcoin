
# FIXME: For OS X, need flag for Framework or dylib
IF(DYNAMIC_LIBCOIN)
    ADD_DEFINITIONS(-DCOINWALLET_LIBRARY)
ELSE()
    ADD_DEFINITIONS(-DCOINWALLET_LIBRARY_STATIC)
ENDIF()

SET(LIB_NAME coinWallet)

SET(HEADER_PATH ${PROJECT_SOURCE_DIR}/include/${LIB_NAME})
SET(TARGET_H
    ${HEADER_PATH}/BerkeleyDB.h
    ${HEADER_PATH}/Crypter.h
    ${HEADER_PATH}/CryptoKeyStore.h
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/MerkleTx.h
    ${HEADER_PATH}/serialize.h
    ${HEADER_PATH}/Wallet.h
    ${HEADER_PATH}/WalletDB.h
    ${HEADER_PATH}/WalletTx.h
    ${HEADER_PATH}/WalletRPC.h
    ${LIBCOIN_CONFIG_HEADER}
)

#ADD_LIBRARY(${LIB_NAME}
#    ${LIBCOIN_USER_DEFINED_DYNAMIC_OR_STATIC}
#    ${LIB_PUBLIC_HEADERS}
SET(TARGET_SRC
    BerkeleyDB.cpp
    Crypter.cpp
    CryptoKeyStore.cpp
    MerkleTx.cpp
    Wallet.cpp
    WalletDB.cpp
    WalletTx.cpp
    WalletRPC.cpp
    ${LIBCOIN_VERSIONINFO_RC}
)
SET(TARGET_LIBRARIES coin coinChain coinHTTP)

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

