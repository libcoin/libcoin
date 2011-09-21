
# FIXME: For OS X, need flag for Framework or dylib
IF(DYNAMIC_BITCOIN)
    ADD_DEFINITIONS(-DBTCRPC_LIBRARY)
ELSE()
    ADD_DEFINITIONS(-DBTCRPC_LIBRARY_STATIC)
ENDIF()

SET(LIB_NAME btcRPC)

SET(HEADER_PATH ${PROJECT_SOURCE_DIR}/include/${LIB_NAME})
SET(TARGET_H
    ${HEADER_PATH}/json/json_spirit_error_position.h
    ${HEADER_PATH}/json/json_spirit_reader_template.h
    ${HEADER_PATH}/json/json_spirit_reader.h
    ${HEADER_PATH}/json/json_spirit_stream_reader.h
    ${HEADER_PATH}/json/json_spirit_utils.h
    ${HEADER_PATH}/json/json_spirit_value.h
    ${HEADER_PATH}/json/json_spirit_writer_template.h
    ${HEADER_PATH}/json/json_spirit_writer.h
    ${HEADER_PATH}/json/json_spirit.h
    ${HEADER_PATH}/rpc.h
    ${BITCOIN_CONFIG_HEADER}
)

#ADD_LIBRARY(${LIB_NAME}
#    ${BITCOIN_USER_DEFINED_DYNAMIC_OR_STATIC}
#    ${LIB_PUBLIC_HEADERS}
SET(TARGET_SRC
    rpc.cpp
    ${HEADER_PATH}/json/json_spirit_reader.cpp
    ${HEADER_PATH}/json/json_spirit_value.cpp
    ${HEADER_PATH}/json/json_spirit_writer.cpp
    ${BITCOIN_VERSIONINFO_RC}
)
SET(TARGET_LIBRARIES btc btcNode)

SET(TARGET_EXTERNAL_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}    
    ${MATH_LIBRARY} 
    ${SSL_LIBRARY} 
    ${CRYPTO_LIBRARY} 
    ${BOOST_LIBRARIES} 
    ${DL_LIBRARY}
)

SETUP_LIBRARY(${LIB_NAME})

