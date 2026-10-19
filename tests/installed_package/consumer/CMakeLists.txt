# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

cmake_minimum_required(VERSION 3.23)

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

add_executable(caors_installed_tasks_consumer tasks.cpp)
target_compile_features(caors_installed_tasks_consumer PRIVATE cxx_std_20)
target_link_libraries(caors_installed_tasks_consumer PRIVATE caors::caors)

add_executable(caors_installed_coroutine_consumer coroutine.cpp)
target_compile_features(caors_installed_coroutine_consumer PRIVATE cxx_std_20)
target_link_libraries(caors_installed_coroutine_consumer PRIVATE caors::caors)

add_executable(caors_installed_resources_coroutine_consumer resources_coroutine.cpp)
target_compile_features(caors_installed_resources_coroutine_consumer PRIVATE cxx_std_20)
target_link_libraries(caors_installed_resources_coroutine_consumer PRIVATE caors::caors)

# 静态安装包需要能够私有并入另一个共享库。
add_library(caors_installed_shared_host SHARED shared_host.cpp)
target_link_libraries(caors_installed_shared_host PRIVATE caors::caors)
add_executable(caors_installed_shared_host_consumer shared_host_main.cpp)
target_compile_features(caors_installed_shared_host_consumer PRIVATE cxx_std_20)
get_target_property(CAORS_CONSUMER_INCLUDE_DIRS caors::caors INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(caors_installed_shared_host_consumer PRIVATE ${CAORS_CONSUMER_INCLUDE_DIRS})
target_link_libraries(caors_installed_shared_host_consumer PRIVATE caors_installed_shared_host)

add_library(caors_installed_static_host STATIC shared_host.cpp)
target_link_libraries(caors_installed_static_host PRIVATE caors::caors)
add_executable(caors_installed_static_host_consumer shared_host_main.cpp)
target_compile_features(caors_installed_static_host_consumer PRIVATE cxx_std_20)
target_include_directories(caors_installed_static_host_consumer PRIVATE ${CAORS_CONSUMER_INCLUDE_DIRS})
target_link_libraries(caors_installed_static_host_consumer PRIVATE caors_installed_static_host)

enable_testing()
add_test(NAME caors.installed_static_host_consumer COMMAND caors_installed_static_host_consumer)
add_test(NAME caors.installed_shared_host_consumer COMMAND caors_installed_shared_host_consumer)
add_test(NAME caors.installed_consumer COMMAND caors_installed_consumer)
add_test(NAME caors.installed_tasks_consumer COMMAND caors_installed_tasks_consumer)
add_test(NAME caors.installed_coroutine_consumer COMMAND caors_installed_coroutine_consumer)
add_test(
  NAME caors.installed_resources_coroutine_consumer
  COMMAND caors_installed_resources_coroutine_consumer
)

if(CAORS_INSTALLED_SHARED)
  if(NOT CAORS_INSTALLED_RUNTIME_DIR)
    message(FATAL_ERROR "Shared consumer 缺少 CAORS_INSTALLED_RUNTIME_DIR")
  endif()

  if(WIN32)
    set(CAORS_INSTALLED_RUNTIME_VARIABLE PATH)
  elseif(APPLE)
    set(CAORS_INSTALLED_RUNTIME_VARIABLE DYLD_LIBRARY_PATH)
  else()
    set(CAORS_INSTALLED_RUNTIME_VARIABLE LD_LIBRARY_PATH)
  endif()

  foreach(
    CONSUMER_TEST
    IN ITEMS
      caors.installed_consumer
      caors.installed_tasks_consumer
      caors.installed_coroutine_consumer
      caors.installed_resources_coroutine_consumer
      caors.installed_shared_host_consumer
      caors.installed_static_host_consumer
  )
    set_property(
      TEST ${CONSUMER_TEST}
      PROPERTY ENVIRONMENT_MODIFICATION
               "${CAORS_INSTALLED_RUNTIME_VARIABLE}=path_list_prepend:${CAORS_INSTALLED_RUNTIME_DIR}"
    )
  endforeach()
endif()
