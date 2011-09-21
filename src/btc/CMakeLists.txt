
# FIXME: For OS X, need flag for Framework or dylib
IF(DYNAMIC_BITCOIN)
    ADD_DEFINITIONS(-DBTC_LIBRARY)
ELSE()
    ADD_DEFINITIONS(-DBTC_LIBRARY_STATIC)
ENDIF()

SET(LIB_NAME btc)

#
# Enable workaround for OpenGL driver crash with occlusion query
#
OPTION(BTC_FORCE_QUERY_RESULT_AVAILABLE_BEFORE_RETRIEVAL "Set to ON to build OcclussionQueryNode with a workaround for multi-threaded OpenGL driver occlussion query crash. " OFF)
IF(BTC_FORCE_QUERY_RESULT_AVAILABLE_BEFORE_RETRIEVAL)
    ADD_DEFINITIONS(-DFORCE_QUERY_RESULT_AVAILABLE_BEFORE_RETRIEVAL)
ENDIF()


SET(HEADER_PATH ${PROJECT_SOURCE_DIR}/include/${LIB_NAME})
SET(TARGET_H
    ${HEADER_PATH}/base58.h
    ${HEADER_PATH}/bignum.h
    ${HEADER_PATH}/crypter.h
    ${HEADER_PATH}/key.h
    ${HEADER_PATH}/keystore.h
    ${HEADER_PATH}/script.h
    ${HEADER_PATH}/serialize.h
    ${HEADER_PATH}/strlcpy.h
    ${HEADER_PATH}/tx.h
    ${HEADER_PATH}/uint256.h
    ${HEADER_PATH}/util.h
    ${BITCOIN_CONFIG_HEADER}
)

#ADD_LIBRARY(${LIB_NAME}
#    ${BITCOIN_USER_DEFINED_DYNAMIC_OR_STATIC}
#    ${LIB_PUBLIC_HEADERS}
SET(TARGET_SRC
    crypter.cpp
    keystore.cpp
    tx.cpp
    script.cpp
    util.cpp

    ${BITCOIN_VERSIONINFO_RC}
)
SET(TARGET_LIBRARIES )

SET(TARGET_EXTERNAL_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}    
    ${MATH_LIBRARY} 
    ${SSL_LIBRARY} 
    ${CRYPTO_LIBRARY} 
    ${BOOST_LIBRARIES} 
    ${DL_LIBRARY}
)

SETUP_LIBRARY(${LIB_NAME})

