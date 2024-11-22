find_path(VBAN_DIR
        NAMES vban.h
        HINTS ${CMAKE_MODULE_PATH}/../vban
)

mark_as_advanced(VBAN_DIR)
set(VBAN_INCLUDE_DIR ${VBAN_DIR})
set(VBAN_SOURCES
        ${VBAN_DIR}/vbanstreamencoder.cpp
)
set(VBAN_LICENSE_FILES ${VBAN_DIR}/LICENSE)

# promote package for find
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(vban REQUIRED_VARS VBAN_DIR VBAN_INCLUDE_DIR VBAN_SOURCES VBAN_LICENSE_FILES)
