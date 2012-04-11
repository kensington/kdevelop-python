find_library(LIBPYTHON-KDEVELOP python2.7-kdevelop)

IF (LIBPYTHON-KDEVELOP)
    SET(LIBPYTHON-KDEVELOP_FOUND TRUE)
ENDIF (LIBPYTHON-KDEVELOP)

IF (LIBPYTHON-KDEVELOP_FOUND)
    MESSAGE(STATUS "Found python-kdevelop: ${LIBPYTHON-KDEVELOP}")
ELSE (LIBPYTHON-KDEVELOP_FOUND)
    MESSAGE(FATAL_ERROR "Missing required python-kdevelop")
ENDIF (LIBPYTHON-KDEVELOP_FOUND)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(parser_STAT_SRCS
    codehelpers.cpp
    parsesession.cpp
    ast.cpp
    astdefaultvisitor.cpp
    astvisitor.cpp
    astbuilder.cpp
)

kde4_add_library( kdev4pythonparser SHARED ${parser_SRCS} ${parser_STAT_SRCS} )
target_link_libraries(kdev4pythonparser 
    ${KDE4_KDECORE_LIBS} 
    ${KDEVPLATFORM_LANGUAGE_LIBRARIES}
    ${QT_QTCORE_LIBRARY} 
    ${LIBPYTHON-KDEVELOP}
)

install(TARGETS kdev4pythonparser DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS})

