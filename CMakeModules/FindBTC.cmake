# Locate gdal
# This module defines
# BTC_LIBRARY
# BTC_FOUND, if false, do not try to link to gdal 
# BTC_INCLUDE_DIR, where to find the headers
#
# $BTC_DIR is an environment variable that would
# correspond to the ./configure --prefix=$BTC_DIR
#
# Created by Robert Osfield. 

FIND_PATH(BTC_INCLUDE_DIR btc/uint256.h
    ${BTC_DIR}/include
    $ENV{BTC_DIR}/include
    $ENV{BTC_DIR}
    $ENV{BTCDIR}/include
    $ENV{BTCDIR}
    $ENV{BTC_ROOT}/include
    NO_DEFAULT_PATH
)

FIND_PATH(BTC_INCLUDE_DIR btc/uint256.h)

MACRO(FIND_BTC_LIBRARY MYLIBRARY MYLIBRARYNAME)

    FIND_LIBRARY("${MYLIBRARY}_DEBUG"
        NAMES "${MYLIBRARYNAME}${CMAKE_DEBUG_POSTFIX}"
        PATHS
        ${BTC_DIR}/lib/Debug
        ${BTC_DIR}/lib64/Debug
        ${BTC_DIR}/lib
        ${BTC_DIR}/lib64
        $ENV{BTC_DIR}/lib/debug
        $ENV{BTC_DIR}/lib64/debug
        $ENV{BTC_DIR}/lib
        $ENV{BTC_DIR}/lib64
        $ENV{BTC_DIR}
        $ENV{BTCDIR}/lib
        $ENV{BTCDIR}/lib64
        $ENV{BTCDIR}
        $ENV{BTC_ROOT}/lib
        $ENV{BTC_ROOT}/lib64
        NO_DEFAULT_PATH
    )

    FIND_LIBRARY("${MYLIBRARY}_DEBUG"
        NAMES "${MYLIBRARYNAME}${CMAKE_DEBUG_POSTFIX}"
        PATHS
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/lib
        /usr/local/lib64
        /usr/lib
        /usr/lib64
        /sw/lib
        /opt/local/lib
        /opt/csw/lib
        /opt/lib
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment;BTC_ROOT]/lib
        /usr/freeware/lib64
    )
    
    FIND_LIBRARY(${MYLIBRARY}
        NAMES "${MYLIBRARYNAME}${CMAKE_RELEASE_POSTFIX}"
        PATHS
        ${BTC_DIR}/lib/Release
        ${BTC_DIR}/lib64/Release
        ${BTC_DIR}/lib
        ${BTC_DIR}/lib64
        $ENV{BTC_DIR}/lib/Release
        $ENV{BTC_DIR}/lib64/Release
        $ENV{BTC_DIR}/lib
        $ENV{BTC_DIR}/lib64
        $ENV{BTC_DIR}
        $ENV{BTCDIR}/lib
        $ENV{BTCDIR}/lib64
        $ENV{BTCDIR}
        $ENV{BTC_ROOT}/lib
        $ENV{BTC_ROOT}/lib64
        NO_DEFAULT_PATH
    )

    FIND_LIBRARY(${MYLIBRARY}
        NAMES "${MYLIBRARYNAME}${CMAKE_RELEASE_POSTFIX}"
        PATHS
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/lib
        /usr/local/lib64
        /usr/lib
        /usr/lib64
        /sw/lib
        /opt/local/lib
        /opt/csw/lib
        /opt/lib
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment;BTC_ROOT]/lib
        /usr/freeware/lib64
    )
    
    IF( NOT ${MYLIBRARY}_DEBUG)
        IF(MYLIBRARY)
            SET(${MYLIBRARY}_DEBUG ${MYLIBRARY})
         ENDIF(MYLIBRARY)
    ENDIF( NOT ${MYLIBRARY}_DEBUG)
           
ENDMACRO(FIND_BTC_LIBRARY LIBRARY LIBRARYNAME)

FIND_BTC_LIBRARY(BTC_LIBRARY btc)
FIND_BTC_LIBRARY(BTCNODE_LIBRARY btcNode)
FIND_BTC_LIBRARY(BTCRPC_LIBRARY btcRPC)
FIND_BTC_LIBRARY(BTCWALLET_LIBRARY btcWallet)
FIND_BTC_LIBRARY(BTCMINE_LIBRARY btcMine)

SET(BTC_FOUND "NO")
IF(BTC_LIBRARY AND BTC_INCLUDE_DIR)
    SET(BTC_FOUND "YES")
ENDIF(BTC_LIBRARY AND BTC_INCLUDE_DIR)
