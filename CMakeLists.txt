cmake_minimum_required(VERSION 2.8)
project(iwraw C)

include(CheckIncludeFiles)
include(CheckTypeSize)
include(FindPkgConfig)

set(IWRAW_VERSION "0.1")

#
# iwraw configuration
#
option(IWRAW_DEBUG "Add debug info to binaries." ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

#
# Dependency check
#
message("Using pkg-config: " ${PKG_CONFIG_EXECUTABLE})

# Check for libnl-genl
pkg_check_modules(LIBNL REQUIRED libnl-genl-3.0>=3.0)

link_directories(${LIBNL_LIBRARY_DIRS})
include_directories(${LIBNL_INCLUDE_DIRS})

if (NOT LIBNL_FOUND)
	message(FATAL_ERROR "Missing dependecies")
endif()

# Check for h-files
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdbool.h HAVE_STDBOOL_H)
check_include_files(stdio.h HAVE_STDIO_H)
check_include_files(stdlib.h HAVE_STDLIB_H)
check_include_files(string.h HAVE_STRING_H)
check_include_files(errno.h HAVE_ERRNO_H)
check_include_files(getopt.h HAVE_GETOPT_H)

# Check sizes of data types
check_type_size(int64_t INT64_T)
check_type_size("long long" LONG_LONG)
if (NOT HAVE_INT64_T AND (NOT HAVE_LONG_LONG OR NOT (${LONG_LONG} EQUAL 8) ))
	message(FATAL_ERROR "Missing 64 bit integer type!")
endif()
check_type_size(int32_t INT32_T)
check_type_size(long LONG)
if (NOT HAVE_INT32_T AND (NOT HAVE_LONG OR NOT (${LONG} EQUAL 4) ))
	message(FATAL_ERROR "Missing 32 bit integer type!")
endif()
check_type_size(uint32_t UINT32_T)
check_type_size("unsigned long" UNSIGNED_LONG)
if (NOT HAVE_UINT32_T AND (NOT HAVE_UNSIGNED_LONG OR NOT (${UNSIGNED_LONG} EQUAL 4) ))
	message(FATAL_ERROR "Missing unsigned 32 bit integer type!")
endif()
check_type_size(uint16_t UINT16_T)
check_type_size("unsigned short" UNSIGNED_SHORT)
if (NOT HAVE_UINT16_T AND (NOT HAVE_UNSIGNED_SHORT OR NOT (${UNSIGNED_SHORT} EQUAL 2) ))
	message(FATAL_ERROR "Missing unsigned 16 bit integer type!")
endif()
check_type_size(uint8_t UINT8_T)
check_type_size("unsigned char" UNSIGNED_CHAR)
if (NOT HAVE_UINT8_T AND (NOT HAVE_UNSIGNED_CHAR OR NOT (${UNSIGNED_CHAR} EQUAL 1) ))
	message(FATAL_ERROR "Missing unsigned 8 bit integer type!")
endif()
check_type_size(ssize_t SSIZE_T)
if (NOT HAVE_SSIZE_T)
	message(FATAL_ERROR "Missing ssize_t!")
endif()
check_type_size(size_t SIZE_T)
if (NOT HAVE_SIZE_T)
	message(FATAL_ERROR "Missing size_t!")
endif()

#
# Prepare build / define targets
#

set(GIT_SHA_AVAILABLE 0)
find_program(GIT git)

if (GIT)
	execute_process(
		COMMAND ${GIT} log -1 --format=%h
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		RESULT_VARIABLE GIT_CMD_RESULT
		OUTPUT_VARIABLE GIT_COMMIT_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if (NOT GIT_CMD_RESULT)
		set(GIT_SHA_AVAILABLE 1)
	endif()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/iwraw_config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/iwraw_config.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(IWRAW_SRC src/iwraw.c src/genl.c src/util.c)

add_executable(iwraw ${IWRAW_SRC})
target_link_libraries(iwraw ${LIBNL_LIBRARIES})

if (CMAKE_COMPILER_IS_GNUCC)
	add_definitions(-Wall -Wextra -Wdeclaration-after-statement)
endif()

if (IWRAW_DEBUG)
	add_definitions(-g -O0)
endif()

# Allow the user to override installation directories.
set(IWRAW_INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")

set(prefix ${CMAKE_INSTALL_PREFIX})
set(INSTALL_LIB_DIR ${CMAKE_INSTALL_PREFIX}/${IWRAW_INSTALL_LIB_DIR})

#
# Install
#

install(TARGETS iwraw
        RUNTIME DESTINATION "${IWRAW_INSTALL_BIN_DIR}" COMPONENT bin)

