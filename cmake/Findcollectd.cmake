IF(NOT COLLECTD_INCLUDE_DIR)
    FIND_PATH(COLLECTD_INCLUDE_DIR collectd.h PATH_SUFFIXES collectd/core collectd/core/daemon)
    MESSAGE(STATUS "Find Header Directory for collectd: " ${COLLECTD_INCLUDE_DIR})
ENDIF()



