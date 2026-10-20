# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

cmake_minimum_required(VERSION 3.23)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/caors_version.cmake")

project(
  caors
  VERSION "${CAORS_PROJECT_VERSION}"
  DESCRIPTION "C++20 infrastructure library"
  LANGUAGES C CXX
)

option(CAORS_BUILD_TESTING "构建 CAORS 测试" ${PROJECT_IS_TOP_LEVEL})
option(CAORS_BUILD_EXAMPLES "构建 CAORS 示例" ${PROJECT_IS_TOP_LEVEL})
option(CAORS_BUILD_EXAMPLE_TESTING "测试构建中同时构建 CAORS 示例" ON)
option(CAORS_BUILD_BENCHMARKS "构建 CAORS benchmark 程序" OFF)
option(CAORS_ENABLE_ASAN "启用 AddressSanitizer" OFF)
option(
  CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE
  "让 CAORS 内部分配跟随默认资源，仅用于诊断与 benchmark"
  OFF
)
# 显式链接类型只影响 CAORS，AUTO 跟随父工程。
set(CAORS_LIBRARY_TYPE "AUTO" CACHE STRING "CAORS 链接类型：AUTO、STATIC 或 SHARED")
set_property(CACHE CAORS_LIBRARY_TYPE PROPERTY STRINGS AUTO STATIC SHARED)
if(NOT CAORS_LIBRARY_TYPE MATCHES "^(AUTO|STATIC|SHARED)$")
  message(FATAL_ERROR "CAORS_LIBRARY_TYPE 必须为 AUTO、STATIC 或 SHARED")
endif()
if(CAORS_LIBRARY_TYPE STREQUAL "AUTO")
  if(BUILD_SHARED_LIBS)
    set(CAORS_EFFECTIVE_LIBRARY_TYPE SHARED)
  else()
    set(CAORS_EFFECTIVE_LIBRARY_TYPE STATIC)
  endif()
else()
  set(CAORS_EFFECTIVE_LIBRARY_TYPE "${CAORS_LIBRARY_TYPE}")
endif()
set(CAORS_BUILD_SHARED OFF CACHE INTERNAL "CAORS 实际链接模式" FORCE)
if(CAORS_EFFECTIVE_LIBRARY_TYPE STREQUAL "SHARED")
  set(CAORS_BUILD_SHARED ON CACHE INTERNAL "CAORS 实际链接模式" FORCE)
endif()

# 旧选项仅作为首次配置的默认值，新名称始终优先。
foreach(CAORS_OPTION IN ITEMS USE_DEBUG_OUTPUT USE_LOG USE_LOG_SEVERITY)
  if(DEFINED ${CAORS_OPTION})
    message(DEPRECATION "${CAORS_OPTION} 已弃用，请使用 CAORS_${CAORS_OPTION}")
    if(NOT DEFINED CAORS_${CAORS_OPTION})
      set(CAORS_${CAORS_OPTION} "${${CAORS_OPTION}}")
    endif()
  endif()
endforeach()
option(CAORS_USE_DEBUG_OUTPUT "启用调试输出" OFF)
option(CAORS_USE_LOG "启用日志" OFF)
set(CAORS_USE_LOG_SEVERITY "${CAORS_USE_LOG_SEVERITY}" CACHE STRING "日志最低级别")
if(CAORS_USE_LOG_SEVERITY STREQUAL "")
  set(CAORS_USE_LOG_SEVERITY "0" CACHE STRING "日志最低级别" FORCE)
endif()
if(NOT CAORS_USE_LOG_SEVERITY MATCHES "^[0-9]+$")
  message(FATAL_ERROR "CAORS_USE_LOG_SEVERITY 必须为非负整数")
endif()
include(CTest)

if(CAORS_ENABLE_ASAN AND
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  message(FATAL_ERROR "CAORS_ENABLE_ASAN 当前仅支持 Clang 和 GCC")
endif()

set(CAORS_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include("${CAORS_ROOT_DIR}/cmake/caors_platform.cmake")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(
  CAORS_INSTALL_CMAKEDIR
  "${CMAKE_INSTALL_LIBDIR}/cmake/caors"
  CACHE STRING
  "CAORS CMake 包配置安装目录"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(caors_warnings)
include(caors_verify_memory_domains)

caors_verify_memory_domains()

add_subdirectory(vendor)
add_subdirectory(src)

if(CAORS_BUILD_EXAMPLES OR (CAORS_BUILD_TESTING AND BUILD_TESTING AND CAORS_BUILD_EXAMPLE_TESTING))
  add_subdirectory(examples)
endif()

if(CAORS_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/caorsConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/caorsConfig.cmake"
  INSTALL_DESTINATION "${CAORS_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/caorsConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMinorVersion
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/caorsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/caorsConfigVersion.cmake"
  DESTINATION "${CAORS_INSTALL_CMAKEDIR}"
)

install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
  DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)

if(CAORS_BUILD_TESTING AND BUILD_TESTING)
  add_subdirectory(tests)
endif()
