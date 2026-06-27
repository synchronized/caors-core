# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

function(caors_add_test_directory TEST_DIRECTORY)
  set(TEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIRECTORY}")
  file(
    GLOB_RECURSE TEST_SOURCES
    CONFIGURE_DEPENDS
    "${TEST_SOURCE_DIR}/*.cpp"
    "${TEST_SOURCE_DIR}/*.hpp"
    "${TEST_SOURCE_DIR}/*.h"
  )

  if(NOT TEST_SOURCES)
    return()
  endif()

  string(MAKE_C_IDENTIFIER "${TEST_DIRECTORY}" TEST_IDENTIFIER)
  set(TARGET_NAME "caors_${TEST_IDENTIFIER}_test")

  add_executable(${TARGET_NAME})
  target_sources(${TARGET_NAME} PRIVATE ${TEST_SOURCES})
  target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
  target_compile_warnings(${TARGET_NAME})

  target_link_libraries(${TARGET_NAME} PRIVATE caors::caors)
  target_link_libraries(${TARGET_NAME} PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

  source_group(TREE "${TEST_SOURCE_DIR}" FILES ${TEST_SOURCES})
  set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Tests")

  add_test(NAME "caors.${TEST_DIRECTORY}" COMMAND ${TARGET_NAME})
endfunction()

file(GLOB CAORS_TEST_DIRECTORIES CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*")
list(SORT CAORS_TEST_DIRECTORIES)

foreach(CAORS_TEST_DIRECTORY IN LISTS CAORS_TEST_DIRECTORIES)
  if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${CAORS_TEST_DIRECTORY}")
    caors_add_test_directory("${CAORS_TEST_DIRECTORY}")
  endif()
endforeach()
