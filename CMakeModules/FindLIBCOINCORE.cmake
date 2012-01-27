# Locate gdal
# This module defines
# LIBCOIN_LIBRARY
# LIBCOIN_FOUND, if false, do not try to link to gdal 
# LIBCOIN_INCLUDE_DIR, where to find the headers
#
# $LIBCOIN_DIR is an environment variable that would
# correspond to the ./configure --prefix=$LIBCOIN_DIR
#
# Created by Robert Osfield. Adapted to libcoin by Michael Gronager

FIND_PATH(LIBCOIN_INCLUDE_DIR coin/uint256.h
    ${LIBCOIN_DIR}/include
    $ENV{LIBCOIN_DIR}/include
    $ENV{LIBCOIN_DIR}
    $ENV{LIBCOINDIR}/include
    $ENV{LIBCOINDIR}
    $ENV{LIBCOIN_ROOT}/include
    NO_DEFAULT_PATH
)

FIND_PATH(LIBCOIN_INCLUDE_DIR coin/uint256.h)

MACRO(FIND_LIBCOIN_LIBRARY MYLIBRARY MYLIBRARYNAME)

    FIND_LIBRARY("${MYLIBRARY}_DEBUG"
        NAMES "${MYLIBRARYNAME}${CMAKE_DEBUG_POSTFIX}"
        PATHS
        ${LIBCOIN_DIR}/lib/Debug
        ${LIBCOIN_DIR}/lib64/Debug
        ${LIBCOIN_DIR}/lib
        ${LIBCOIN_DIR}/lib64
        $ENV{LIBCOIN_DIR}/lib/debug
        $ENV{LIBCOIN_DIR}/lib64/debug
        $ENV{LIBCOIN_DIR}/lib
        $ENV{LIBCOIN_DIR}/lib64
        $ENV{LIBCOIN_DIR}
        $ENV{LIBCOINDIR}/lib
        $ENV{LIBCOINDIR}/lib64
        $ENV{LIBCOINDIR}
        $ENV{LIBCOIN_ROOT}/lib
        $ENV{LIBCOIN_ROOT}/lib64
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
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment;LIBCOIN_ROOT]/lib
        /usr/freeware/lib64
    )
    
    FIND_LIBRARY(${MYLIBRARY}
        NAMES "${MYLIBRARYNAME}${CMAKE_RELEASE_POSTFIX}"
        PATHS
        ${LIBCOIN_DIR}/lib/Release
        ${LIBCOIN_DIR}/lib64/Release
        ${LIBCOIN_DIR}/lib
        ${LIBCOIN_DIR}/lib64
        $ENV{LIBCOIN_DIR}/lib/Release
        $ENV{LIBCOIN_DIR}/lib64/Release
        $ENV{LIBCOIN_DIR}/lib
        $ENV{LIBCOIN_DIR}/lib64
        $ENV{LIBCOIN_DIR}
        $ENV{LIBCOINDIR}/lib
        $ENV{LIBCOINDIR}/lib64
        $ENV{LIBCOINDIR}
        $ENV{LIBCOIN_ROOT}/lib
        $ENV{LIBCOIN_ROOT}/lib64
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
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment;LIBCOIN_ROOT]/lib
        /usr/freeware/lib64
    )
    
    IF( NOT ${MYLIBRARY}_DEBUG)
        IF(MYLIBRARY)
            SET(${MYLIBRARY}_DEBUG ${MYLIBRARY})
         ENDIF(MYLIBRARY)
    ENDIF( NOT ${MYLIBRARY}_DEBUG)
           
ENDMACRO(FIND_LIBCOIN_LIBRARY LIBRARY LIBRARYNAME)

FIND_LIBCOIN_LIBRARY(COIN_LIBRARY coin)
FIND_LIBCOIN_LIBRARY(COINCHAIN_LIBRARY coinChain)
FIND_LIBCOIN_LIBRARY(COINHTTP_LIBRARY coinHTTP)
FIND_LIBCOIN_LIBRARY(COINWALLET_LIBRARY coinWallet)
FIND_LIBCOIN_LIBRARY(COINMINE_LIBRARY coinMine)

SET(LIBCOIN_FOUND "NO")
IF(LIBCOIN_LIBRARY AND LIBCOIN_INCLUDE_DIR)
    SET(LIBCOIN_FOUND "YES")
ENDIF(LIBCOIN_LIBRARY AND LIBCOIN_INCLUDE_DIR)
