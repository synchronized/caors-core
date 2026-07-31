# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

cmake_minimum_required(VERSION 3.24)

project(caors_installed_consumer LANGUAGES CXX)

find_package(
  caors
  CONFIG
  REQUIRED
  NO_CMAKE_PACKAGE_REGISTRY
  NO_CMAKE_SYSTEM_PACKAGE_REGISTRY
)

if(NOT TARGET caors::caors)
  message(FATAL_ERROR "安装包没有导出 caors::caors")
endif()

message(STATUS "使用 CAORS package: ${caors_DIR}")

add_executable(caors_installed_consumer main.cpp)
target_compile_features(caors_installed_consumer PRIVATE cxx_std_20)
target_link_libraries(caors_installed_consumer PRIVATE caors::caors)

enable_testing()
add_test(NAME caors.installed_consumer COMMAND caors_installed_consumer)
