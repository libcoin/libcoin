
# FIXME: For OS X, need flag for Framework or dylib
IF(DYNAMIC_LIBCOIN)
    ADD_DEFINITIONS(-DCOINNAT_LIBRARY)
ELSE()
    ADD_DEFINITIONS(-DCOINNAT_LIBRARY_STATIC)
ENDIF()

SET(LIB_NAME coinNAT)

SET(HEADER_PATH ${PROJECT_SOURCE_DIR}/include/${LIB_NAME})
SET(TARGET_H
    miniupnpc/bsdqueue.h
    miniupnpc/connecthostport.h
    miniupnpc/igd_desc_parse.h
    miniupnpc/minisoap.h
    miniupnpc/minissdpc.h
    miniupnpc/miniupnpc.h
    miniupnpc/miniwget.h
    miniupnpc/minixml.h
    miniupnpc/portlistingparse.h
    miniupnpc/upnpcommands.h
    miniupnpc/upnperrors.h
    miniupnpc/upnpreplyparse.h
    miniupnpc/miniupnpcstrings.h
    miniupnpc/miniwget.h
    miniupnpc/declspec.h
    miniupnpc/miniupnpctypes.h
    miniupnpc/codelength.h
    miniupnpc/receivedata.h
    
    libnatpmp/getgateway.h
    libnatpmp/natpmp.h
    libnatpmp/wingettimeofday.h
    libnatpmp/declspec.h

    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/PortMapper.h
    ${LIBCOIN_CONFIG_HEADER}
)

#ADD_LIBRARY(${LIB_NAME}
#    ${LIBCOIN_USER_DEFINED_DYNAMIC_OR_STATIC}
#    ${LIB_PUBLIC_HEADERS}
SET(TARGET_SRC
    miniupnpc/connecthostport.c
    miniupnpc/igd_desc_parse.c
    miniupnpc/minisoap.c
    miniupnpc/minissdpc.c
    miniupnpc/miniupnpc.c
    miniupnpc/miniwget.c
    miniupnpc/minixml.c
    miniupnpc/portlistingparse.c
    miniupnpc/upnpc.c
    miniupnpc/upnpcommands.c
    miniupnpc/upnperrors.c
    miniupnpc/upnpreplyparse.c
    miniupnpc/receivedata.c

    libnatpmp/getgateway.c
    libnatpmp/natpmp.c
    libnatpmp/wingettimeofday.c

    PortMapper.cpp

    ${LIBCOIN_VERSIONINFO_RC}
)

SOURCE_GROUP("miniupnpc" FILES
    miniupnpc/bsdqueue.h
    miniupnpc/connecthostport.h
    miniupnpc/igd_desc_parse.h
    miniupnpc/minisoap.h
    miniupnpc/minissdpc.h
    miniupnpc/miniupnpc.h
    miniupnpc/miniwget.h
    miniupnpc/minixml.h
    miniupnpc/portlistingparse.h
    miniupnpc/upnpcommands.h
    miniupnpc/upnperrors.h
    miniupnpc/upnpreplyparse.h
    miniupnpc/miniupnpcstrings.h
    miniupnpc/miniwget.h
    miniupnpc/declspec.h
    miniupnpc/miniupnpctypes.h
    miniupnpc/codelength.h
    miniupnpc/receivedata.h

    miniupnpc/connecthostport.c
    miniupnpc/igd_desc_parse.c
    miniupnpc/minisoap.c
    miniupnpc/minissdpc.c
    miniupnpc/miniupnpc.c
    miniupnpc/miniwget.c
    miniupnpc/minixml.c
    miniupnpc/portlistingparse.c
    miniupnpc/upnpc.c
    miniupnpc/upnpcommands.c
    miniupnpc/upnperrors.c
    miniupnpc/upnpreplyparse.c    
    miniupnpc/receivedata.c
    
)

SOURCE_GROUP("libnatpmp" FILES
    libnatpmp/getgateway.h
    libnatpmp/natpmp.h
    libnatpmp/wingettimeofday.h
    libnatpmp/declspec.h

    libnatpmp/getgateway.c
    libnatpmp/natpmp.c
    libnatpmp/wingettimeofday.c
)

SET(TARGET_LIBRARIES coin coinChain coinHTTP)

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

