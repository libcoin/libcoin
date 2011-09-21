# Locate Berkeley DB C++ 
# This module defines
# BOOST_LIBRARY
# BOOST_FOUND, if false, do not try to link to zlib 
# BOOST_INCLUDE_DIR, where to find the headers
#
# $BOOST_DIR is an environment variable that would
# correspond to the ./configure --prefix=$BOOST_DIR
# used in building BOOST.
#
# Created by Michael Gronager. 

FIND_PATH(BOOST_INCLUDE_DIR boost/foreach.hpp
    $ENV{BOOST_DIR}/include
    $ENV{BOOST_DIR}
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/include
    /usr/include
    /sw/include # Fink
    /opt/local/include # DarwinPorts
    /opt/csw/include # Blastwave
    /opt/include
    /usr/freeware/include
)

FIND_LIBRARY(BOOST_SYSTEM_LIBRARY 
NAMES boost_system libboost_system
PATHS
$ENV{BOOST_DIR}/lib
$ENV{BOOST_DIR}
~/Library/Frameworks
/Library/Frameworks
/usr/local/lib
/usr/lib
/sw/lib
/opt/local/lib
/opt/csw/lib
/opt/lib
/usr/freeware/lib64
)

FIND_LIBRARY(BOOST_FILESYSTEM_LIBRARY 
NAMES boost_filesystem libboost_filesystem
PATHS
$ENV{BOOST_DIR}/lib
$ENV{BOOST_DIR}
~/Library/Frameworks
/Library/Frameworks
/usr/local/lib
/usr/lib
/sw/lib
/opt/local/lib
/opt/csw/lib
/opt/lib
/usr/freeware/lib64
)

FIND_LIBRARY(BOOST_THREAD_LIBRARY 
NAMES boost_thread libboost_thread
PATHS
$ENV{BOOST_DIR}/lib
$ENV{BOOST_DIR}
~/Library/Frameworks
/Library/Frameworks
/usr/local/lib
/usr/lib
/sw/lib
/opt/local/lib
/opt/csw/lib
/opt/lib
/usr/freeware/lib64
)

FIND_LIBRARY(BOOST_PROGRAM_OPTIONS_LIBRARY 
NAMES boost_program_options libboost_program_options
PATHS
$ENV{BOOST_DIR}/lib
$ENV{BOOST_DIR}
~/Library/Frameworks
/Library/Frameworks
/usr/local/lib
/usr/lib
/sw/lib
/opt/local/lib
/opt/csw/lib
/opt/lib
/usr/freeware/lib64
)

SET(BOOST_LIBRARIES ${BOOST_SYSTEM_LIBRARY} ${BOOST_FILESYSTEM_LIBRARY} ${BOOST_THREAD_LIBRARY} ${BOOST_PROGRAM_OPTIONS_LIBRARY})

SET(BOOST_FOUND "NO")
IF(BOOST_LIBRARIES AND BOOST_INCLUDE_DIR)
    SET(BOOST_FOUND "YES")
ENDIF(BOOST_LIBRARIES AND BOOST_INCLUDE_DIR)


