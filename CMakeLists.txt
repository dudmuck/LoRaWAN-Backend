cmake_minimum_required (VERSION 2.6)

macro(use_c99)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "--std=gnu99 ${CMAKE_C_FLAGS}")
    endif ()
  else ()
    set (CMAKE_C_STANDARD 99)
  endif ()
endmacro(use_c99)

project (lora C)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS_DEBUG "-g -O2 -Wall")
execute_process(COMMAND mariadb_config --include OUTPUT_VARIABLE DBINC OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CMAKE_C_FLAGS  "-O2 -Wall ${DBINC}")
# The version number.
set (lora_VERSION_MAJOR 1)
set (lora_VERSION_MINOR 0)
 
# configure a header file to pass some of the CMake settings
# to the source code
#configure_file (
#  "${PROJECT_SOURCE_DIR}/lora_config.h.in"
#  "${PROJECT_BINARY_DIR}/lora_config.h"
#  )
 
# add the binary tree to the search path for include files
# so that we will find lora_classCConfig.h
#include_directories("${PROJECT_BINARY_DIR}")

#add_definitions(-DUSE_BAND_433)

add_library(libbackend SHARED
    libbackend/libserver.c
    libbackend/aes.c
    libbackend/cmac.c
    libbackend/resolve.c
)

find_library( MYSQL_LIBRARY
    NAMES "mysqlclient" "mysqlclient_r" "mariadb"
    PATHS "/lib/mysql"
          "/lib64/mysql"
          "/usr/lib"
          "/usr/lib/mysql"
          "/usr/lib64/mysql"
          "/usr/local/lib/mysql"
          "/usr/local/lib64/mysql"
          "/usr/mysql/lib/mysql"
          "/usr/mysql/lib64/mysql" )
target_link_libraries(libbackend json-c curl microhttpd gcrypt gpg-error ${MYSQL_LIBRARY})

list(APPEND ns_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/web.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/gateway.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/child.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/fNS.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/hNS.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/sNS.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/adr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/band_arib8ch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/band_us902.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/band_au915.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/band_eu868.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/band_eu433.c
    ${CMAKE_CURRENT_SOURCE_DIR}/network_server/band_cn779.c
)

list(APPEND js_sources
	${CMAKE_CURRENT_SOURCE_DIR}/join_server/web.c
	${CMAKE_CURRENT_SOURCE_DIR}/join_server/main.c
)

list(APPEND as_sources
	${CMAKE_CURRENT_SOURCE_DIR}/app_server/web.c
	${CMAKE_CURRENT_SOURCE_DIR}/app_server/child.c
	${CMAKE_CURRENT_SOURCE_DIR}/app_server/main.c
	${CMAKE_CURRENT_SOURCE_DIR}/app_server/base64.c
)

include_directories(libbackend)

#list(APPEND cmd_sources
#	${CMAKE_CURRENT_SOURCE_DIR}/cmd/main.c
#	${CMAKE_CURRENT_SOURCE_DIR}/cmd/pktlog.c
#	${CMAKE_CURRENT_SOURCE_DIR}/cmd/parson.c
#)

use_c99()

# add the executable
add_executable(network_server ${ns_sources})
add_executable(join_server ${js_sources})
add_executable(app_server ${as_sources})
add_executable(resolve ${CMAKE_CURRENT_SOURCE_DIR}/resolve/main.c)

find_library(M_LIB m)
target_link_libraries(join_server libbackend)
target_link_libraries(app_server libbackend rt)
target_link_libraries(resolve resolv libbackend)

find_package (Threads REQUIRED)
message(STATUS "CMAKE_THREAD_LIBS_INIT=${CMAKE_THREAD_LIBS_INIT}")
target_link_libraries(network_server ${MYSQL_LIB} libbackend m rt ${CMAKE_THREAD_LIBS_INIT})

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
option(BUILD_DOCUMENTATION "Create and install the HTML based API documentation (requires Doxygen)" ${DOXYGEN_FOUND})

if(BUILD_DOCUMENTATION)
    if(NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Doxygen is needed to build the documentation.")
    endif()

    set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${doxyfile_in} ${doxyfile} @ONLY)

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)

    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION share/doc)
endif()

