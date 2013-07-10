# Locate Berkeley DB C++ 
# This module defines
# BDB_LIBRARY
# BDB_FOUND, if false, do not try to link to zlib 
# BDB_INCLUDE_DIR, where to find the headers
#
# $BDB_DIR is an environment variable that would
# correspond to the ./configure --prefix=$BDB_DIR
# used in building BDB.
#
# Created by Michael Gronager. 

FIND_PATH(BDB_INCLUDE_DIR db_cxx.h
    $ENV{BDB_DIR}/include
    $ENV{BDB_DIR}
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/include
    /usr/include
    /usr/local/opt/berkeley-db/include  # HomeBrew
    /usr/local/opt/berkeley-db4/include # HomeBrew, BDB4
    /sw/include # Fink
    /opt/local/include # DarwinPorts
    /opt/csw/include # Blastwave
    /opt/include
    /usr/freeware/include
)

FIND_LIBRARY(BDB_LIBRARY 
    NAMES db_cxx libdb_cxx
    PATHS
    $ENV{BDB_DIR}/lib
    $ENV{BDB_DIR}
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/lib
    /usr/lib
    /usr/local/opt/berkeley-db/lib  # HomeBrew
    /usr/local/opt/berkeley-db4/lib # HomeBrew, BDB4
    /sw/lib
    /opt/local/lib
    /opt/csw/lib
    /opt/lib
    /usr/freeware/lib64
)

SET(BDB_FOUND "NO")
IF(BDB_LIBRARY AND BDB_INCLUDE_DIR)
    SET(BDB_FOUND "YES")
ENDIF(BDB_LIBRARY AND BDB_INCLUDE_DIR)


