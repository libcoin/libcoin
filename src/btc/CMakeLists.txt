
# FIXME: For OS X, need flag for Framework or dylib
IF(DYNAMIC_LIBCOIN)
    ADD_DEFINITIONS(-DCOIN_LIBRARY)
ELSE()
    ADD_DEFINITIONS(-DCOIN_LIBRARY_STATIC)
ENDIF()

SET(LIB_NAME coin)

SET(HEADER_PATH ${PROJECT_SOURCE_DIR}/include/${LIB_NAME})
SET(TARGET_H
    ${HEADER_PATH}/bignum.h
    ${HEADER_PATH}/serialize.h
    ${HEADER_PATH}/uint256.h
    ${HEADER_PATH}/util.h
    
    ${HEADER_PATH}/asset.h
    ${HEADER_PATH}/Address.h
    ${HEADER_PATH}/Block.h
    ${HEADER_PATH}/key.h
    ${HEADER_PATH}/keystore.h
    ${HEADER_PATH}/script.h
    ${HEADER_PATH}/Transaction.h
    ${LIBCOIN_CONFIG_HEADER}
)

#ADD_LIBRARY(${LIB_NAME}
#    ${LIBCOIN_USER_DEFINED_DYNAMIC_OR_STATIC}
#    ${LIB_PUBLIC_HEADERS}
SET(TARGET_SRC
    util.cpp
    
    Address.cpp
    asset.cpp
    Block.cpp
    keystore.cpp
    Transaction.cpp
    script.cpp
    ${LIBCOIN_VERSIONINFO_RC}
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

