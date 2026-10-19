# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

if(NOT CAORS_BUILD_SHARED)
  message(STATUS "跳过 plugin protocol 测试：该测试要求 CAORS_BUILD_SHARED=ON")
  return()
endif()

set(
  PLUGIN_TARGETS
  caors_plugin_protocol_good
  caors_plugin_protocol_second_good
  caors_plugin_protocol_bad_abi
  caors_plugin_protocol_missing_entry
  caors_plugin_protocol_failure
  caors_plugin_protocol_incompatible_build
  caors_plugin_protocol_missing_build_extension
)

add_library(caors_plugin_protocol_good SHARED good_plugin.cpp)
add_library(caors_plugin_protocol_second_good SHARED second_good_plugin.cpp)
add_library(caors_plugin_protocol_bad_abi SHARED bad_abi_plugin.cpp)
add_library(caors_plugin_protocol_missing_entry SHARED missing_entry_plugin.c)
add_library(caors_plugin_protocol_failure SHARED failure_plugin.cpp)
add_library(caors_plugin_protocol_incompatible_build SHARED incompatible_build_plugin.cpp)
add_library(caors_plugin_protocol_missing_build_extension SHARED
            missing_build_extension_plugin.cpp)

foreach(PLUGIN_TARGET IN LISTS PLUGIN_TARGETS)
  target_compile_features(${PLUGIN_TARGET} PRIVATE cxx_std_20)
  caors_target_compile_warnings(${PLUGIN_TARGET})
  target_link_libraries(${PLUGIN_TARGET} PRIVATE caors::caors)
  # 插件内部模板与静态状态必须隔离，只有显式标记的 C ABI 入口对外可见。
  set_target_properties(${PLUGIN_TARGET} PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
  )
  # GNU unique 会使 dlclose 跳过真实卸载，不能用于可卸载插件。
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(${PLUGIN_TARGET} PRIVATE
      $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-fno-gnu-unique>)
  endif()
  set_target_properties(${PLUGIN_TARGET} PROPERTIES FOLDER "Tests/Plugins")
endforeach()

if(MSVC)
  foreach(PLUGIN_TARGET IN LISTS PLUGIN_TARGETS)
    set_property(
      TARGET ${PLUGIN_TARGET}
      PROPERTY MSVC_RUNTIME_LIBRARY
               "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
  endforeach()
endif()

add_executable(caors_plugins_protocol_test plugin_binary_test.cpp)
target_compile_features(caors_plugins_protocol_test PRIVATE cxx_std_20)
caors_target_compile_warnings(caors_plugins_protocol_test)
target_link_libraries(caors_plugins_protocol_test PRIVATE caors::caors)
if(MSVC)
  set_property(
    TARGET caors_plugins_protocol_test
    PROPERTY MSVC_RUNTIME_LIBRARY
             "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
  )
endif()
add_dependencies(caors_plugins_protocol_test ${PLUGIN_TARGETS})
set_target_properties(caors_plugins_protocol_test PROPERTIES FOLDER "Tests")

if(WIN32)
  add_custom_command(
    TARGET caors_plugins_protocol_test
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:caors>
      $<TARGET_FILE_DIR:caors_plugins_protocol_test>
  )
endif()

add_test(
  NAME caors.plugins.protocol
  COMMAND
    caors_plugins_protocol_test $<TARGET_FILE:caors_plugin_protocol_good>
    $<TARGET_FILE:caors_plugin_protocol_second_good>
    $<TARGET_FILE:caors_plugin_protocol_bad_abi>
    $<TARGET_FILE:caors_plugin_protocol_missing_entry>
    $<TARGET_FILE:caors_plugin_protocol_failure>
    $<TARGET_FILE:caors_plugin_protocol_incompatible_build>
    $<TARGET_FILE:caors_plugin_protocol_missing_build_extension>
)

add_executable(caors_plugins_resource_lifetime_test resource_lifetime_test.cpp)
target_compile_features(caors_plugins_resource_lifetime_test PRIVATE cxx_std_20)
caors_target_compile_warnings(caors_plugins_resource_lifetime_test)
if(MSVC)
  set_property(TARGET caors_plugins_resource_lifetime_test PROPERTY MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()
target_link_libraries(caors_plugins_resource_lifetime_test PRIVATE caors::caors)
add_dependencies(caors_plugins_resource_lifetime_test caors_plugin_protocol_good)
if(WIN32)
  add_custom_command(TARGET caors_plugins_resource_lifetime_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:caors>
      $<TARGET_FILE_DIR:caors_plugins_resource_lifetime_test>)
endif()
add_test(NAME caors.plugins.resource_lifetime
  COMMAND caors_plugins_resource_lifetime_test $<TARGET_FILE:caors_plugin_protocol_good>)
