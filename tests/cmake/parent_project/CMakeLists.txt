# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

cmake_minimum_required(VERSION 3.23)
project(caors_parent_contract VERSION 9.8.7 LANGUAGES C CXX)

if(NOT DEFINED CAORS_SOURCE_DIR)
  message(FATAL_ERROR "缺少 CAORS_SOURCE_DIR")
endif()

# 模拟父工程的通用函数，确保 add_subdirectory() 不会覆盖上游命令。
function(target_compile_warnings target)
  set_property(TARGET ${target} PROPERTY CAORS_HOST_WARNING_HELPER_CALLED TRUE)
endfunction()

set(CAORS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CAORS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CAORS_BUILD_EXAMPLE_TESTING OFF CACHE BOOL "" FORCE)
set(CAORS_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(CAORS_ENABLE_WARNINGS ON CACHE BOOL "" FORCE)

if(DEFINED CAORS_TEST_LIBRARY_TYPE)
  set(CAORS_LIBRARY_TYPE "${CAORS_TEST_LIBRARY_TYPE}" CACHE STRING "" FORCE)
endif()
if(CAORS_TEST_LIBRARY_TYPE STREQUAL "STATIC")
  set(BUILD_SHARED_LIBS ON)
elseif(CAORS_TEST_LIBRARY_TYPE STREQUAL "SHARED")
  set(BUILD_SHARED_LIBS OFF)
endif()
add_subdirectory("${CAORS_SOURCE_DIR}" caors)
get_target_property(actual_type caors TYPE)
if(DEFINED CAORS_TEST_LIBRARY_TYPE AND NOT actual_type STREQUAL "${CAORS_TEST_LIBRARY_TYPE}_LIBRARY")
  message(FATAL_ERROR "显式链接模式未生效：${actual_type}")
endif()
if(TARGET Catch2::Catch2 OR TARGET ctrack::ctrack)
  message(FATAL_ERROR "关闭测试和基准后不应创建对应依赖")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/host_probe.cpp"
     "// SPDX-License-Identifier: MIT\n"
     "// Copyright (c) 2026 CAORS contributors\n\n"
     "int caors_host_probe() { return 0; }\n")
add_library(host_probe STATIC "${CMAKE_CURRENT_BINARY_DIR}/host_probe.cpp")
target_compile_warnings(host_probe)
get_target_property(host_helper_called host_probe CAORS_HOST_WARNING_HELPER_CALLED)
if(NOT host_helper_called)
  message(FATAL_ERROR "CAORS 覆盖了父工程的 target_compile_warnings()")
endif()

caors_target_compile_warnings(host_probe)
get_target_property(private_options host_probe COMPILE_OPTIONS)
get_target_property(interface_options host_probe INTERFACE_COMPILE_OPTIONS)
if(NOT private_options OR interface_options)
  message(FATAL_ERROR "CAORS 警告选项必须只作用于当前目标，不能泄漏到下游")
endif()

if(NOT PROJECT_VERSION STREQUAL "9.8.7")
  message(FATAL_ERROR "CAORS 修改了父工程的 PROJECT_VERSION")
endif()

include("${CAORS_SOURCE_DIR}/cmake/caors_version.cmake")
get_directory_property(caors_project_version DIRECTORY "${CAORS_SOURCE_DIR}" DEFINITION PROJECT_VERSION)
if(NOT caors_project_version STREQUAL CAORS_PROJECT_VERSION)
  message(FATAL_ERROR "CAORS 子工程未使用唯一版本来源")
endif()

file(READ "${CMAKE_CURRENT_BINARY_DIR}/caors/src/generated/include/caors/version.h" version_header)
string(FIND "${version_header}" "k_version_string = \"${CAORS_PROJECT_VERSION}\"" version_position)
file(READ "${CMAKE_CURRENT_BINARY_DIR}/caors/caorsConfigVersion.cmake" package_version)
string(FIND "${package_version}" "set(PACKAGE_VERSION \"${CAORS_PROJECT_VERSION}\")" package_position)
if(version_position LESS 0 OR package_position LESS 0)
  message(FATAL_ERROR "生成头或安装包版本未与工程版本一致")
endif()
