IF(ANDROID)
    SET(MODULE_USER_STATIC_OR_DYNAMIC ${LIBCOIN_USER_DEFINED_DYNAMIC_OR_STATIC})
ENDIF(ANDROID)

IF(MSVC80 OR MSVC90)
  OPTION(OSG_MSVC_GENERATE_PLUGINS_AND_WRAPPERS_MANIFESTS "Generate or not manifests files under VS8 for dynamically loaded dlls" ON)
ENDIF()

#the old construct SUBDIRS( was substituded by ADD_SUBDIRECTORY that is to be preferred according on CMake docs.
FOREACH( mylibfolder 
        coin
        coinChain 
        coinHTTP
        coinNAT
        coinName
        coinWallet 
        coinPool 
    )

    ADD_SUBDIRECTORY(${mylibfolder})

ENDFOREACH()

#IF (QT4_FOUND AND NOT ANDROID)
#    ADD_SUBDIRECTORY(coinQt)
#ENDIF()

IF(ANDROID)
    configure_file("${LIBCOIN_ANDROID_TEMPLATES}/Android.mk.src.in" "${CMAKE_CURRENT_BINARY_DIR}/Android.mk")
ENDIF()

