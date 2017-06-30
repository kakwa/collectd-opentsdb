cmake_minimum_required (VERSION 2.6)
project (writer_tsdb2)
include(ExternalProject)

set(VERSION 0.0.1)

option(DEBUG              "compile with debug symbol"    OFF)
option(LINK_DL            "link dl" OFF)
option(LINK_GCC_S         "link gcc_s" OFF)
option(LINK_PTHREAD       "link pthread" OFF)

IF(STATIC)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    set(OPENSSL_USE_STATIC_LIBS ON)
    set(ARGP_USE_STATIC_LIBS ON)
    set(CIVETWEB_USE_STATIC_LIBS ON)
ENDIF(STATIC)

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

#find_package(OpenSSL REQUIRED)
#find_package(LibreSSL)

MESSAGE(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
MESSAGE(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")
MESSAGE(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
MESSAGE(STATUS "LibreSSL Detected: ${IS_LIBRESSL}")
MESSAGE(STATUS "OS Detected: ${CMAKE_SYSTEM_NAME}")


#if(NOT(DEFINED OPENSSL_API_1_1))
#    if(${OPENSSL_VERSION} VERSION_GREATER 1.0.99 AND NOT(IS_LIBRESSL))
#        set(OPENSSL_API_1_1 ON)
#    else()
#        set(OPENSSL_API_1_1 OFF)
#    endif()
#endif()



add_library(write_tsdb2
    "SHARED"
    src/write_tsdb2.c
)

SET_TARGET_PROPERTIES(write_tsdb2 PROPERTIES PREFIX "")

target_link_libraries(write_tsdb2
    ${OPENSSL_LIBRARIES}
    ${DL_LIBRARIES}
    ${GCC_S_LIBRARIES}
    ${PTHREAD_LIBRARIES}
)

INSTALL(TARGETS write_tsdb2
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

if(BUNDLE_CIVETWEB)
    add_dependencies(writer_tsdb2 civetweb)
endif(BUNDLE_CIVETWEB)
