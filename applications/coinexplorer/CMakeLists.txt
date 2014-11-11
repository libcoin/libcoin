SET(TARGET_SRC coinexplorer.cpp)

SOURCE_GROUP("Document Root\\explorer" FILES
www/explorer/index.html
www/explorer/demo.css
www/explorer/demo-print.css
)

SOURCE_GROUP("Document Root\\css" FILES
www/css/demo.css
www/css/demo-print.css
)

SOURCE_GROUP("Document Root\\js" FILES
www/js/raphael.js_
)

SET(TARGET_SRC
coinexplorer.cpp
www/explorer/index.html
www/css/demo.css
www/css/demo-print.css
www/js/raphael.js_
)


SET(TARGET_ADDED_LIBRARIES coinNAT coinName)

IF( CMAKE_SYSTEM MATCHES "Linux" )
    SET(TARGET_EXTERNAL_LIBRARIES
        ${CMAKE_THREAD_LIBS_INIT}    
        ${MATH_LIBRARY} 
        ${OPENSSL_LIBRARIES} 
        ${Boost_LIBRARIES} 
        ${BDB_LIBRARY} 
        ${SQLITE3_LIBRARIES}
        ${DL_LIBRARY}
        ${RT_LIBRARY}
    )
ELSE( CMAKE_SYSTEM MATCHES "Linux" )
    SET(TARGET_EXTERNAL_LIBRARIES
        ${CMAKE_THREAD_LIBS_INIT}    
        ${MATH_LIBRARY} 
        ${OPENSSL_LIBRARIES} 
        ${Boost_LIBRARIES} 
        ${BDB_LIBRARY} 
        ${SQLITE3_LIBRARIES}
        ${DL_LIBRARY}
    )
ENDIF( CMAKE_SYSTEM MATCHES "Linux" )

SETUP_APPLICATION(coinexplorer)
