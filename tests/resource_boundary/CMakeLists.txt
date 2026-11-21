# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

cmake_minimum_required(VERSION 3.23)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(caors_resource_boundary LANGUAGES CXX)
  enable_testing()
endif()

get_filename_component(CAORS_BOUNDARY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CAORS_BOUNDARY_POISON "${CMAKE_CURRENT_BINARY_DIR}/forbidden")
file(GLOB_RECURSE CAORS_BOUNDARY_IO_HEADERS CONFIGURE_DEPENDS RELATIVE "${CAORS_BOUNDARY_ROOT}/include"
     "${CAORS_BOUNDARY_ROOT}/include/caors/io/*.h")
list(APPEND CAORS_BOUNDARY_IO_HEADERS "caors/io.h")
foreach(CAORS_BOUNDARY_HEADER IN LISTS CAORS_BOUNDARY_IO_HEADERS)
  get_filename_component(CAORS_BOUNDARY_PARENT "${CAORS_BOUNDARY_HEADER}" DIRECTORY)
  file(MAKE_DIRECTORY "${CAORS_BOUNDARY_POISON}/${CAORS_BOUNDARY_PARENT}")
  file(WRITE "${CAORS_BOUNDARY_POISON}/${CAORS_BOUNDARY_HEADER}"
       "#error CAORS_RESOURCE_BOUNDARY_FORBIDDEN_IO\n")
endforeach()

# 只编译，不链接单体 caors；P0 不把编译边界误记为独立链接已完成。
add_library(caors_resource_boundary OBJECT core.cpp)
target_compile_features(caors_resource_boundary PRIVATE cxx_std_20)
target_compile_definitions(caors_resource_boundary PRIVATE CAORS_STATIC_DEFINE)
target_include_directories(caors_resource_boundary BEFORE PRIVATE
  "${CAORS_BOUNDARY_POISON}" "${CAORS_BOUNDARY_ROOT}/include")

# P2 的运行原型仅编译必要的基础设施，不链接 caors 单体或任何 IO/项目装配实现。
add_library(caors_resource_boundary_support STATIC
  "${CAORS_BOUNDARY_ROOT}/src/caors/memory/resource.cpp"
  "${CAORS_BOUNDARY_ROOT}/src/caors/memory/detail/internal.cpp"
  "${CAORS_BOUNDARY_ROOT}/src/caors/tasks/executor.cpp"
  "${CAORS_BOUNDARY_ROOT}/src/caors/tasks/execution_context.cpp"
  "${CAORS_BOUNDARY_ROOT}/src/caors/core/string_name.cpp"
  "${CAORS_BOUNDARY_ROOT}/src/caors/core/guid.cpp")
target_compile_features(caors_resource_boundary_support PUBLIC cxx_std_20)
target_compile_definitions(caors_resource_boundary_support PUBLIC CAORS_STATIC_DEFINE)
target_include_directories(caors_resource_boundary_support BEFORE PUBLIC
  "${CAORS_BOUNDARY_POISON}" "${CAORS_BOUNDARY_ROOT}/include")
if(NOT EMSCRIPTEN)
  find_package(Threads REQUIRED)
  target_link_libraries(caors_resource_boundary_support PUBLIC Threads::Threads)
endif()
add_executable(caors_resource_boundary_runtime runtime.cpp)
target_link_libraries(caors_resource_boundary_runtime PRIVATE caors_resource_boundary_support)
add_test(NAME caors.resource_boundary.runtime COMMAND caors_resource_boundary_runtime)

# 独立目标不能靠链接 caors 继承 sanitizer；必须让模板、支持实现和最终链接一起受检。
if(CAORS_ENABLE_ASAN)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|GNU)$")
    message(FATAL_ERROR "资源边界探针的 ASan 仅支持 Clang 和 GCC")
  endif()
  target_compile_options(caors_resource_boundary PRIVATE -fsanitize=address -fno-omit-frame-pointer)
  target_compile_options(caors_resource_boundary_support PUBLIC -fsanitize=address -fno-omit-frame-pointer)
  target_link_options(caors_resource_boundary_support INTERFACE -fsanitize=address)
endif()

# 当前数据库与装配仍应命中被禁止的 IO 头；P2 迁移时须将其改为正向探针。
foreach(CAORS_BOUNDARY_ENTRY IN ITEMS database assets)
  if(CAORS_BOUNDARY_ENTRY STREQUAL "database")
    set(CAORS_BOUNDARY_INCLUDE "caors/resources/database.h")
  else()
    set(CAORS_BOUNDARY_INCLUDE "caors/assets/asset_manager.h")
  endif()
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${CAORS_BOUNDARY_ENTRY}.cpp"
       "#include <${CAORS_BOUNDARY_INCLUDE}>\n")
  try_compile(CAORS_BOUNDARY_ACCEPTED
    "${CMAKE_CURRENT_BINARY_DIR}/probe-${CAORS_BOUNDARY_ENTRY}"
    "${CMAKE_CURRENT_BINARY_DIR}/${CAORS_BOUNDARY_ENTRY}.cpp"
    CMAKE_FLAGS "-DCMAKE_CXX_STANDARD=20" "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
      "-DINCLUDE_DIRECTORIES=${CAORS_BOUNDARY_POISON};${CAORS_BOUNDARY_ROOT}/include"
    COMPILE_DEFINITIONS -DCAORS_STATIC_DEFINE
    OUTPUT_VARIABLE CAORS_BOUNDARY_OUTPUT)
  if(CAORS_BOUNDARY_ACCEPTED OR NOT CAORS_BOUNDARY_OUTPUT MATCHES "CAORS_RESOURCE_BOUNDARY_FORBIDDEN_IO")
    message(FATAL_ERROR "${CAORS_BOUNDARY_ENTRY} 基线变化或探针配置失败：${CAORS_BOUNDARY_OUTPUT}")
  endif()
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${CAORS_BOUNDARY_ENTRY}-baseline.log"
       "${CAORS_BOUNDARY_OUTPUT}")
  message(STATUS "资源边界基线：${CAORS_BOUNDARY_ENTRY} 仍依赖 IO，诊断已记录")
endforeach()
