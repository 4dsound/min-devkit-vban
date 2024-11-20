# Copyright 2018 The Min-DevKit Authors. All rights reserved.
# Use of this source code is governed by the MIT License found in the License.md file.

cmake_minimum_required(VERSION 3.0)

set(C74_MIN_API_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../min-api)
include(${C74_MIN_API_DIR}/script/min-pretarget.cmake)

set(THIRDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/../thirdparty)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${THIRDPARTY_DIR}/cmake_find_modules)

#############################################################
# MAX EXTERNAL
#############################################################

find_package(asio REQUIRED)

include_directories( 
	"${C74_INCLUDES}"
)


set( SOURCE_FILES
	${PROJECT_NAME}.h
	${PROJECT_NAME}.cpp
)


add_library( 
	${PROJECT_NAME} 
	MODULE
	${SOURCE_FILES}
	vban.h
)


include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)

target_include_directories(${PROJECT_NAME} PUBLIC src ${ASIO_INCLUDE_DIR})
target_compile_definitions(${PROJECT_NAME} PUBLIC ASIO_STANDALONE)

# additional definitions
if(WIN32)
	target_compile_definitions(${PROJECT_NAME} PUBLIC WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
endif()



#############################################################
# UNIT TEST
#############################################################

include(${C74_MIN_API_DIR}/test/min-object-unittest.cmake)
