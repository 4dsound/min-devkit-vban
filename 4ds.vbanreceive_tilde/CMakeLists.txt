# Copyright 2018 The Min-DevKit Authors. All rights reserved.
# Use of this source code is governed by the MIT License found in the License.md file.

cmake_minimum_required(VERSION 3.0)

set(C74_MIN_API_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../min-api)
include(${C74_MIN_API_DIR}/script/min-pretarget.cmake)

# Import thirdparty directory
set(THIRDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/../thirdparty)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${THIRDPARTY_DIR}/cmake_find_modules)

#############################################################
# MAX EXTERNAL
#############################################################

find_package(vban REQUIRED)


include_directories( 
	"${C74_INCLUDES}"
	${VBAN_INCLUDE_DIR}
)


set( SOURCE_FILES
	${PROJECT_NAME}.cpp
	${VBAN_SOURCES}
)


add_library( 
	${PROJECT_NAME} 
	MODULE
	${SOURCE_FILES}
)


include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)


#############################################################
# UNIT TEST
#############################################################

include(${C74_MIN_API_DIR}/test/min-object-unittest.cmake)
