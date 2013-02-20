
# FIXME: For OS X, need flag for Framework or dylib
IF(DYNAMIC_LIBCOIN)
    ADD_DEFINITIONS(-DCOINHTTP_LIBRARY)
ELSE()
    ADD_DEFINITIONS(-DCOINHTTP_LIBRARY_STATIC)
ENDIF()

SET(LIB_NAME coinHTTP)
SET(SUB_LIB_NAME json)

SET(HEADER_PATH ${PROJECT_SOURCE_DIR}/include/${LIB_NAME})

SET(SUB_TARGET_H
    ${HEADER_PATH}/json/json_spirit_error_position.h
    ${HEADER_PATH}/json/json_spirit_reader_template.h
    ${HEADER_PATH}/json/json_spirit_reader.h
    ${HEADER_PATH}/json/json_spirit_stream_reader.h
    ${HEADER_PATH}/json/json_spirit_utils.h
    ${HEADER_PATH}/json/json_spirit_value.h
    ${HEADER_PATH}/json/json_spirit_writer_template.h
    ${HEADER_PATH}/json/json_spirit_writer.h
    ${HEADER_PATH}/json/json_spirit.h
)

SET(TARGET_H
    ${HEADER_PATH}/Client.h
    ${HEADER_PATH}/ConnectionManager.h
    ${HEADER_PATH}/Connection.h
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/Header.h
    ${HEADER_PATH}/Method.h
    ${HEADER_PATH}/MimeTypes.h
    ${HEADER_PATH}/Reply.h
    ${HEADER_PATH}/Request.h
    ${HEADER_PATH}/RequestHandler.h
    ${HEADER_PATH}/RequestParser.h
    ${HEADER_PATH}/RPC.h
    ${HEADER_PATH}/Server.h
    ${LIBCOIN_CONFIG_HEADER}
)

SET(TARGET_SRC
    ${HEADER_PATH}/json/json_spirit_reader.cpp
    ${HEADER_PATH}/json/json_spirit_value.cpp
    ${HEADER_PATH}/json/json_spirit_writer.cpp
    Client.cpp
    ConnectionManager.cpp
    Connection.cpp
    Method.cpp
    MimeTypes.cpp
    Reply.cpp
    Request.cpp
    RequestHandler.cpp
    RequestParser.cpp
    RPC.cpp
    Server.cpp
    ${LIBCOIN_VERSIONINFO_RC}
)

SET(TARGET_LIBRARIES coin)

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


