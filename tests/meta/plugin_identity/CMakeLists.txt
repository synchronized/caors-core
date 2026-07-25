# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

if(NOT BUILD_SHARED_LIBS)
  message(STATUS "跳过 meta plugin identity 测试：该测试要求 BUILD_SHARED_LIBS=ON")
  return()
endif()

set(PLUGIN_SUPPORT_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/plugin_identity_test_support.h")

add_library(caors_meta_identity_plugin_a SHARED plugin_a.cpp ${PLUGIN_SUPPORT_HEADER})
add_library(caors_meta_identity_plugin_b SHARED plugin_b.cpp ${PLUGIN_SUPPORT_HEADER})

foreach(PLUGIN_TARGET IN ITEMS caors_meta_identity_plugin_a caors_meta_identity_plugin_b)
  target_compile_features(${PLUGIN_TARGET} PRIVATE cxx_std_20)
  target_compile_warnings(${PLUGIN_TARGET})
  target_include_directories(${PLUGIN_TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(${PLUGIN_TARGET} PRIVATE caors::caors)
  set_target_properties(${PLUGIN_TARGET} PROPERTIES FOLDER "Tests/Plugins")
endforeach()

add_executable(caors_meta_plugin_identity_test plugin_identity_test.cpp ${PLUGIN_SUPPORT_HEADER})
target_compile_features(caors_meta_plugin_identity_test PRIVATE cxx_std_20)
target_compile_warnings(caors_meta_plugin_identity_test)
target_include_directories(caors_meta_plugin_identity_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(caors_meta_plugin_identity_test PRIVATE caors::caors ${CMAKE_DL_LIBS})
add_dependencies(caors_meta_plugin_identity_test caors_meta_identity_plugin_a
                 caors_meta_identity_plugin_b)
set_target_properties(caors_meta_plugin_identity_test PROPERTIES FOLDER "Tests")

if(WIN32)
  add_custom_command(
    TARGET caors_meta_plugin_identity_test
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:caors>
      $<TARGET_FILE_DIR:caors_meta_plugin_identity_test>
  )
endif()

add_test(
  NAME caors.meta.plugin_identity
  COMMAND
    caors_meta_plugin_identity_test $<TARGET_FILE:caors_meta_identity_plugin_a>
    $<TARGET_FILE:caors_meta_identity_plugin_b>
)
