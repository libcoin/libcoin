#######################################################
# this are setting used in SETUP_TESTS macro
#######################################################
SET(TARGET_DEFAULT_PREFIX "test_")
SET(TARGET_DEFAULT_LABEL_PREFIX "Tests")

SET(TARGET_COMMON_LIBRARIES 
    coin
    coinHTTP
    coinChain
)

IF(ANDROID)
    # ADD_SUBDIRECTORY(coinandroidsimple)
ELSE(ANDROID)

IF(DYNAMIC_LIBCOIN)

ELSE(DYNAMIC_LIBCOIN)
    #needed on win32 or the linker get confused by _declspec declarations
    ADD_DEFINITIONS(-DLIBCOIN_LIBRARY_STATIC)
ENDIF(DYNAMIC_LIBCOIN)

    
ADD_SUBDIRECTORY(blocktree)
ADD_SUBDIRECTORY(claims)
ADD_SUBDIRECTORY(client)
ADD_SUBDIRECTORY(extended_key)
ADD_SUBDIRECTORY(jsonance)
ADD_SUBDIRECTORY(merkletrie)
ADD_SUBDIRECTORY(namecoin)
ADD_SUBDIRECTORY(sqliterate)
ADD_SUBDIRECTORY(serialize)

#    IF   (wxWidgets_FOUND)
#        ADD_SUBDIRECTORY(bitsimpleWX)
#    ENDIF(wxWidgets_FOUND)

ENDIF(ANDROID)

