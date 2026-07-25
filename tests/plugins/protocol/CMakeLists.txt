# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

if(NOT BUILD_SHARED_LIBS)
  message(STATUS "跳过 plugin protocol 测试：该测试要求 BUILD_SHARED_LIBS=ON")
  return()
endif()

set(
  PLUGIN_TARGETS
  caors_plugin_protocol_good
  caors_plugin_protocol_bad_abi
  caors_plugin_protocol_missing_entry
  caors_plugin_protocol_failure
)

add_library(caors_plugin_protocol_good SHARED good_plugin.cpp)
add_library(caors_plugin_protocol_bad_abi SHARED bad_abi_plugin.cpp)
add_library(caors_plugin_protocol_missing_entry SHARED missing_entry_plugin.c)
add_library(caors_plugin_protocol_failure SHARED failure_plugin.cpp)

foreach(PLUGIN_TARGET IN LISTS PLUGIN_TARGETS)
  target_compile_features(${PLUGIN_TARGET} PRIVATE cxx_std_20)
  target_compile_warnings(${PLUGIN_TARGET})
  target_link_libraries(${PLUGIN_TARGET} PRIVATE caors::caors)
  set_target_properties(${PLUGIN_TARGET} PROPERTIES FOLDER "Tests/Plugins")
endforeach()

add_executable(caors_plugins_protocol_test plugin_binary_test.cpp)
target_compile_features(caors_plugins_protocol_test PRIVATE cxx_std_20)
target_compile_warnings(caors_plugins_protocol_test)
target_link_libraries(caors_plugins_protocol_test PRIVATE caors::caors)
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
    $<TARGET_FILE:caors_plugin_protocol_bad_abi>
    $<TARGET_FILE:caors_plugin_protocol_missing_entry>
    $<TARGET_FILE:caors_plugin_protocol_failure>
)
