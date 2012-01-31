#######################################################
# this are setting used in SETUP_EXAMPLE macro
#######################################################
SET(TARGET_DEFAULT_PREFIX "example_")
SET(TARGET_DEFAULT_LABEL_PREFIX "Examples")

SET(TARGET_COMMON_LIBRARIES 
    coin
    coinHTTP
    coinChain
)

IF(ANDROID)
    # ADD_SUBDIRECTORY(coinandroidsimple)
ELSE(ANDROID)

IF(DYNAMIC_LIBCOIN)
    ADD_SUBDIRECTORY(simplecoin)
    ADD_SUBDIRECTORY(ponzicoin)
#    ADD_SUBDIRECTORY(coinexplorer)
#    ADD_SUBDIRECTORY(coinonline)

    IF   (wxWidgets_FOUND)
#        ADD_SUBDIRECTORY(bitsimpleWX)
    ENDIF(wxWidgets_FOUND)


ELSE(DYNAMIC_LIBCOIN)
    #needed on win32 or the linker get confused by _declspec declarations
    ADD_DEFINITIONS(-DLIBCOIN_LIBRARY_STATIC)
    
    ADD_SUBDIRECTORY(simplecoin)
    ADD_SUBDIRECTORY(ponzicoin)
ENDIF(DYNAMIC_LIBCOIN)

ENDIF(ANDROID)

