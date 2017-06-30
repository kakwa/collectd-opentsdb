cmake_minimum_required (VERSION 2.6)
project (writer_tsdb2)
include(ExternalProject)

set(VERSION 0.0.1)

option(DEBUG              "compile with debug symbol"    OFF)
option(LINK_DL            "link dl" OFF)
option(LINK_GCC_S         "link gcc_s" OFF)
option(LINK_PTHREAD       "link pthread" OFF)

IF(LINK_DL)
    set(DL_LIBRARIES 'dl')
ENDIF(LINK_DL)

IF(LINK_GCC_S)
    set(GCC_S_LIBRARIES 'gcc_s')
ENDIF(LINK_GCC_S)

IF(LINK_PTHREAD)
    set(PTHREAD_LIBRARIES 'pthread')
ENDIF(LINK_PTHREAD)

set (CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700")

if(DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
    set(CMAKE_BUILD_TYPE Debug)
endif(DEBUG)


add_custom_target(tag 
    git tag -a "${VERSION}" -m "version ${VERSION}" &&
    git push origin ${VERSION}
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTSBD_WRITER2_VERSION='\"${VERSION}\"'")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(CURL REQUIRED)
find_package(collectd REQUIRED)

MESSAGE(STATUS "Collectd include dir: ${COLLECTD_INCLUDE_DIR}")
MESSAGE(STATUS "Curl include directory: ${CURL_INCLUDE_DIRS}")
MESSAGE(STATUS "Curl library: ${CURL_LIBRARIES}")
MESSAGE(STATUS "Curl version: ${CURL_VERSION_STRING}")
MESSAGE(STATUS "OS Detected: ${CMAKE_SYSTEM_NAME}")

add_library(write_tsdb2
    "SHARED"
    src/write_tsdb2.c
)

SET_TARGET_PROPERTIES(write_tsdb2 PROPERTIES PREFIX "")

target_link_libraries(write_tsdb2
    ${CURL_LIBRARIES}
    ${DL_LIBRARIES}
    ${GCC_S_LIBRARIES}
    ${PTHREAD_LIBRARIES}
)

INSTALL(TARGETS write_tsdb2
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
