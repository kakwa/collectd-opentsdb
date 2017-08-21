cmake_minimum_required (VERSION 2.6)
project (write_opentsdb)
include(ExternalProject)

set(VERSION 0.0.6)

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

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700")

if(DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
    set(CMAKE_BUILD_TYPE Debug)
endif(DEBUG)

add_custom_target(tag
    git tag -a "${VERSION}" -m "version ${VERSION}" &&
    git push origin ${VERSION}
)


add_custom_target(man ALL)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTSBD_WRITER2_VERSION='\"${VERSION}\"'")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(CURL REQUIRED)
find_package(collectd REQUIRED)
find_package(JSON-C REQUIRED)

INCLUDE(Pod2Man)

POD2MAN(${CMAKE_CURRENT_SOURCE_DIR}/docs/collectd-opentsdb.pod collectd-opentsdb 5)

MESSAGE(STATUS "Collectd include dir: ${COLLECTD_INCLUDE_DIR}")
MESSAGE(STATUS "Curl include directory: ${CURL_INCLUDE_DIRS}")
MESSAGE(STATUS "Curl library: ${CURL_LIBRARIES}")
MESSAGE(STATUS "Curl version: ${CURL_VERSION_STRING}")
MESSAGE(STATUS "OS Detected: ${CMAKE_SYSTEM_NAME}")

include_directories(
    inc/
    ${CMAKE_INCLUDE_PATH}
    ${COLLECTD_INCLUDE_DIR}
    ${COLLECTD_INCLUDE_DIR_BASE}
    ${CURL_INCLUDE_DIRS}
    ${JSON-C_INCLUDE_DIRS}
)

add_library(write_opentsdb
    "SHARED"
    src/write_opentsdb.c
)

ADD_DEFINITIONS(-std=c99)
SET_TARGET_PROPERTIES(write_opentsdb PROPERTIES PREFIX "")

target_link_libraries(write_opentsdb
    ${CURL_LIBRARIES}
    ${DL_LIBRARIES}
    ${GCC_S_LIBRARIES}
    ${PTHREAD_LIBRARIES}
    ${JSON-C_LIBRARIES}
)

INSTALL(TARGETS write_opentsdb
  LIBRARY DESTINATION lib/collectd
  ARCHIVE DESTINATION lib/collectd
)
