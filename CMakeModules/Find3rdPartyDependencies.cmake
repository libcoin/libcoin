################################################################################################
# this Macro find a generic dependency, handling debug suffix
# all the paramenter are required, in case of lists, use "" in calling
################################################################################################

MACRO(FIND_DEPENDENCY DEPNAME INCLUDEFILE LIBRARY_NAMES_BASE SEARCHPATHLIST DEBUGSUFFIX EXSUFFIX)

    MESSAGE(STATUS "searching ${DEPNAME} -->${INCLUDEFILE}<-->${LIBRARY_NAMES_BASE}<-->${SEARCHPATHLIST}<--")

    SET(MY_PATH_INCLUDE )
    SET(MY_PATH_LIB )
    
    FOREACH( MYPATH ${SEARCHPATHLIST} )
        SET(MY_PATH_INCLUDE ${MY_PATH_INCLUDE} ${MYPATH}/include)
        SET(MY_PATH_LIB ${MY_PATH_LIB} ${MYPATH}/lib)
    ENDFOREACH( MYPATH ${SEARCHPATHLIST} )
    
    FIND_PATH("${DEPNAME}_INCLUDE_DIR" ${INCLUDEFILE}
      ${MY_PATH_INCLUDE}
      NO_DEFAULT_PATH
    )
    MARK_AS_ADVANCED("${DEPNAME}_INCLUDE_DIR")
    #MESSAGE( " ${DEPNAME}_INCLUDE_DIR --> ${${DEPNAME}_INCLUDE_DIR}<--")
    
    SET(LIBRARY_NAMES "")
    FOREACH(LIBNAME ${LIBRARY_NAMES_BASE})
        LIST(APPEND LIBRARY_NAMES "${LIBNAME}${EXSUFFIX}")
    ENDFOREACH(LIBNAME)
    FIND_LIBRARY("${DEPNAME}_LIBRARY"
        NAMES ${LIBRARY_NAMES}
      PATHS ${MY_PATH_LIB}
      NO_DEFAULT_PATH
    )
    SET(LIBRARY_NAMES_DEBUG "")
    FOREACH(LIBNAME ${LIBRARY_NAMES_BASE})
        LIST(APPEND LIBRARY_NAMES_DEBUG "${LIBNAME}${DEBUGSUFFIX}${EXSUFFIX}")
    ENDFOREACH(LIBNAME)
    FIND_LIBRARY("${DEPNAME}_LIBRARY_DEBUG" 
        NAMES ${LIBRARY_NAMES_DEBUG}
      PATHS ${MY_PATH_LIB}
      NO_DEFAULT_PATH
    )
    MARK_AS_ADVANCED("${DEPNAME}_LIBRARY")
    #MESSAGE( " ${DEPNAME}_LIBRARY --> ${${DEPNAME}_LIBRARY}<--")
    SET( ${DEPNAME}_FOUND "NO" )
    IF(${DEPNAME}_INCLUDE_DIR AND ${DEPNAME}_LIBRARY)
      SET( ${DEPNAME}_FOUND "YES" )
      IF(NOT ${DEPNAME}_LIBRARY_DEBUG)
          MESSAGE("-- Warning Debug ${DEPNAME} not found, using: ${${DEPNAME}_LIBRARY}")
          SET(${DEPNAME}_LIBRARY_DEBUG "${${DEPNAME}_LIBRARY}")
      ENDIF(NOT ${DEPNAME}_LIBRARY_DEBUG)
    ENDIF(${DEPNAME}_INCLUDE_DIR AND ${DEPNAME}_LIBRARY)
ENDMACRO(FIND_DEPENDENCY DEPNAME INCLUDEFILE LIBRARY_NAMES_BASE SEARCHPATHLIST DEBUGSUFFIX)


################################################################################################
# this Macro is tailored to 3rdparty binary dependencies distribution
################################################################################################

MACRO(SEARCH_3RDPARTY LIBCOIN_3RDPARTY_BIN)
#        FIND_DEPENDENCY(BOOST_DATE_TIME boost/foreach.h libboost_date_time-vc90-mt-1_48.lib ${LIBCOIN_3RDPARTY_BIN} "D" "")
#        FIND_DEPENDENCY(BOOST_REGEX boost/foreach.h libboost_regex-vc90-mt-1_48.lib ${LIBCOIN_3RDPARTY_BIN} "D" "")
#        FIND_DEPENDENCY(BOOST_FILESYSTEM boost/foreach.h libboost_filesystem-vc90-mt-1_48.lib ${LIBCOIN_3RDPARTY_BIN} "D" "")
#        FIND_DEPENDENCY(BOOST_PROGRAM_OPTIONS boost/foreach.h libboost_program_options-vc90-mt-1_48.lib ${LIBCOIN_3RDPARTY_BIN} "D" "")
#        FIND_DEPENDENCY(BOOST_SYSTEM boost/foreach.h libboost_system-vc90-mt-1_48.lib ${LIBCOIN_3RDPARTY_BIN} "D" "")
#        FIND_DEPENDENCY(BOOST_THREAD boost/foreach.h libboost_thread-vc90-mt-1_48.lib ${LIBCOIN_3RDPARTY_BIN} "D" "")
        
#        FIND_DEPENDENCY(CRYPTO openssl/ecdsa.h "libeay32.lib" ${LIBCOIN_3RDPARTY_BIN} "D" "")
#        FIND_DEPENDENCY(SSL openssl/ecdsa.h "ssleay32.lib" ${LIBCOIN_3RDPARTY_BIN} "D" "")
        
        FIND_DEPENDENCY(BDB db_cxx.h "libdb53.lib" ${LIBCOIN_3RDPARTY_BIN} "D" "")
ENDMACRO(SEARCH_3RDPARTY LIBCOIN_3RDPARTY_BIN)




################################################################################################
# this is code for handling optional 3RDPARTY usage
################################################################################################

OPTION(USE_3RDPARTY_BIN "Set to ON to use the prebuilt dependencies situated side of libcoin source.  Use OFF for avoiding." ON)
IF(USE_3RDPARTY_BIN)

    # Check Architecture
    IF( CMAKE_SIZEOF_VOID_P EQUAL 4 )
        MESSAGE( STATUS "32 bit architecture detected" )
        SET(DESTINATION_ARCH "x86")
    ENDIF()
    IF( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        MESSAGE( STATUS "64 bit architecture detected" )
        SET(DESTINATION_ARCH "x64")
    ENDIF()

    GET_FILENAME_COMPONENT(PARENT_DIR ${PROJECT_SOURCE_DIR} PATH)
    SET(TEST_3RDPARTY_DIR "${PARENT_DIR}/3rdparty")
    IF(NOT EXISTS ${TEST_3RDPARTY_DIR})
        SET(3RDPARTY_DIR_BY_ENV $ENV{LIBCOIN_3RDPARTY_DIR})
        IF(3RDPARTY_DIR_BY_ENV)
            MESSAGE( STATUS "3rdParty-Package ENV variable found:${3RDPARTY_DIR_BY_ENV}/${DESTINATION_ARCH}" )
            SET(TEST_3RDPARTY_DIR "${3RDPARTY_DIR_BY_ENV}/${DESTINATION_ARCH}")
        ELSEIF(MSVC71)
            SET(TEST_3RDPARTY_DIR "${PARENT_DIR}/3rdParty_win32binaries_vs71")
        ELSEIF(MSVC80)
            SET(TEST_3RDPARTY_DIR "${PARENT_DIR}/3rdParty_win32binaries_vs80sp1")
        ELSEIF(MSVC90)
            SET(TEST_3RDPARTY_DIR "${PARENT_DIR}/3rdParty_win32binaries_vs90sp1")
        ENDIF()
    ENDIF(NOT EXISTS ${TEST_3RDPARTY_DIR})
    
    SET(ACTUAL_3RDPARTY_DIR "${TEST_3RDPARTY_DIR}" CACHE PATH "Location of 3rdparty dependencies")
    SET(ACTUAL_3DPARTY_DIR "${ACTUAL_3RDPARTY_DIR}")  # kept for backcompatibility
    IF(EXISTS ${ACTUAL_3RDPARTY_DIR})
        SET (3rdPartyRoot ${ACTUAL_3RDPARTY_DIR})
        SEARCH_3RDPARTY(${ACTUAL_3RDPARTY_DIR})
    ENDIF(EXISTS ${ACTUAL_3RDPARTY_DIR})
ENDIF(USE_3RDPARTY_BIN)
