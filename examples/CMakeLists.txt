#######################################################
# this are setting used in SETUP_EXAMPLE macro
#######################################################
SET(TARGET_DEFAULT_PREFIX "example_")
SET(TARGET_DEFAULT_LABEL_PREFIX "Examples")

SET(TARGET_COMMON_LIBRARIES 
    btc
    btcHTTP
    btcNode
)

IF(ANDROID)
    # ADD_SUBDIRECTORY(btcandroidsimple)
ELSE(ANDROID)

IF(DYNAMIC_BITCOIN)
    ADD_SUBDIRECTORY(btcsimple)
    ADD_SUBDIRECTORY(ponzicoin)
#    ADD_SUBDIRECTORY(btcexplorer)
#    ADD_SUBDIRECTORY(btconline)

    IF   (wxWidgets_FOUND)
#        ADD_SUBDIRECTORY(bitsimpleWX)
    ENDIF(wxWidgets_FOUND)


ELSE(DYNAMIC_BITCOIN)
    #needed on win32 or the linker get confused by _declspec declarations
    ADD_DEFINITIONS(-DBTC_LIBRARY_STATIC)
    
ENDIF(DYNAMIC_BITCOIN)

ENDIF(ANDROID)

