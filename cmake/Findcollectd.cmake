IF(NOT COLLECTD_INCLUDE_DIR)
    FIND_PATH(COLLECTD_INCLUDE_DIR collectd.h PATH_SUFFIXES collectd/core collectd/core/daemon)
    FIND_PATH(COLLECTD_INCLUDE_DIR_BASE core PATH_SUFFIXES collectd)
    MESSAGE(STATUS "Find Header Directory for collectd: " ${COLLECTD_INCLUDE_DIR})
    MESSAGE(STATUS "Find Header Base Directory for collectd: " ${COLLECTD_INCLUDE_DIR_BASE})
ENDIF()



