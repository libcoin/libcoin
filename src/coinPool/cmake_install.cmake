# Install script for directory: /Users/gronager/Development/libcoin/libcoin/src/coinPool

# Set the install prefix
IF(NOT DEFINED CMAKE_INSTALL_PREFIX)
  SET(CMAKE_INSTALL_PREFIX "/usr/local")
ENDIF(NOT DEFINED CMAKE_INSTALL_PREFIX)
STRING(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
IF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  IF(BUILD_TYPE)
    STRING(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  ELSE(BUILD_TYPE)
    SET(CMAKE_INSTALL_CONFIG_NAME "Release")
  ENDIF(BUILD_TYPE)
  MESSAGE(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
ENDIF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)

# Set the component getting installed.
IF(NOT CMAKE_INSTALL_COMPONENT)
  IF(COMPONENT)
    MESSAGE(STATUS "Install component: \"${COMPONENT}\"")
    SET(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  ELSE(COMPONENT)
    SET(CMAKE_INSTALL_COMPONENT)
  ENDIF(COMPONENT)
ENDIF(NOT CMAKE_INSTALL_COMPONENT)

IF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "libcoin")
  IF("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/Users/gronager/Development/libcoin/libcoin/lib/libcoinPoold.dylib")
    IF(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPoold.dylib" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPoold.dylib")
      EXECUTE_PROCESS(COMMAND "/usr/bin/install_name_tool"
        -id "libcoinPoold.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinChaind.dylib" "libcoinChaind.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinHTTPd.dylib" "libcoinHTTPd.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinWalletd.dylib" "libcoinWalletd.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoind.dylib" "libcoind.dylib"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPoold.dylib")
      IF(CMAKE_INSTALL_DO_STRIP)
        EXECUTE_PROCESS(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPoold.dylib")
      ENDIF(CMAKE_INSTALL_DO_STRIP)
    ENDIF()
  ELSEIF("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/Users/gronager/Development/libcoin/libcoin/lib/libcoinPool.dylib")
    IF(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPool.dylib" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPool.dylib")
      EXECUTE_PROCESS(COMMAND "/usr/bin/install_name_tool"
        -id "libcoinPool.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoin.dylib" "libcoin.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinChain.dylib" "libcoinChain.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinHTTP.dylib" "libcoinHTTP.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinWallet.dylib" "libcoinWallet.dylib"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPool.dylib")
      IF(CMAKE_INSTALL_DO_STRIP)
        EXECUTE_PROCESS(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPool.dylib")
      ENDIF(CMAKE_INSTALL_DO_STRIP)
    ENDIF()
  ELSEIF("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/Users/gronager/Development/libcoin/libcoin/lib/libcoinPools.dylib")
    IF(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPools.dylib" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPools.dylib")
      EXECUTE_PROCESS(COMMAND "/usr/bin/install_name_tool"
        -id "libcoinPools.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinChains.dylib" "libcoinChains.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinHTTPs.dylib" "libcoinHTTPs.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinWallets.dylib" "libcoinWallets.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoins.dylib" "libcoins.dylib"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPools.dylib")
      IF(CMAKE_INSTALL_DO_STRIP)
        EXECUTE_PROCESS(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPools.dylib")
      ENDIF(CMAKE_INSTALL_DO_STRIP)
    ENDIF()
  ELSEIF("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/Users/gronager/Development/libcoin/libcoin/lib/libcoinPoolrd.dylib")
    IF(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPoolrd.dylib" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPoolrd.dylib")
      EXECUTE_PROCESS(COMMAND "/usr/bin/install_name_tool"
        -id "libcoinPoolrd.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinChainrd.dylib" "libcoinChainrd.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinHTTPrd.dylib" "libcoinHTTPrd.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinWalletrd.dylib" "libcoinWalletrd.dylib"
        -change "/Users/gronager/Development/libcoin/libcoin/lib/libcoinrd.dylib" "libcoinrd.dylib"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPoolrd.dylib")
      IF(CMAKE_INSTALL_DO_STRIP)
        EXECUTE_PROCESS(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libcoinPoolrd.dylib")
      ENDIF(CMAKE_INSTALL_DO_STRIP)
    ENDIF()
  ENDIF()
ENDIF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "libcoin")

IF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "libcoin-dev")
  FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/coinPool" TYPE FILE FILES
    "/Users/gronager/Development/libcoin/libcoin/include/coinPool/Export.h"
    "/Users/gronager/Development/libcoin/libcoin/include/coinPool/GetBlockTemplate.h"
    "/Users/gronager/Development/libcoin/libcoin/include/coinPool/GetWork.h"
    "/Users/gronager/Development/libcoin/libcoin/include/coinPool/Pool.h"
    "/Users/gronager/Development/libcoin/libcoin/include/coinPool/SubmitBlock.h"
    "/Users/gronager/Development/libcoin/libcoin/include/coin/Config.h"
    )
ENDIF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "libcoin-dev")

