# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

cmake_minimum_required(VERSION 3.23)

project(
  caors
  VERSION 0.1.0
  DESCRIPTION "C++20 infrastructure library"
  LANGUAGES C CXX
)

# By default, MSVC uses the GBK encoding.
# To use UTF-8 without a BOM header,
#     you need to add the following statement.
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

option(CAORS_BUILD_TESTING "构建 CAORS 测试" ON)
option(CAORS_BUILD_EXAMPLES "构建 CAORS 示例" ${PROJECT_IS_TOP_LEVEL})
option(BUILD_SHARED_LIBS "构建共享库" OFF)
option(USE_DEBUG_OUTPUT "启用调试输出" OFF)
option(USE_LOG "启用日志" OFF)
set(USE_LOG_SEVERITY "0" CACHE STRING "日志最低级别")

set(CAORS_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(
  CAORS_INSTALL_CMAKEDIR
  "${CMAKE_INSTALL_LIBDIR}/cmake/caors"
  CACHE STRING
  "CAORS CMake 包配置安装目录"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(utility)

add_subdirectory(3rd)
add_subdirectory(src)

if(CAORS_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/caorsConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/caorsConfig.cmake"
  INSTALL_DESTINATION "${CAORS_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/caorsConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
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

include(CTest)
if(CAORS_BUILD_TESTING AND BUILD_TESTING)
  add_subdirectory(tests)
endif()
