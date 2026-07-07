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
  set(GENERATED_TEST_SOURCES)

  if(TEST_DIRECTORY STREQUAL "headers")
    file(
      GLOB_RECURSE CAORS_HEADER_SELF_CONTAINED_HEADERS
      CONFIGURE_DEPENDS
      "${CAORS_ROOT_DIR}/include/caors/*.h"
      "${CAORS_ROOT_DIR}/include/caors/*.hpp"
    )

    set(CAORS_GENERATED_HEADER_TEST_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/generated/header_self_contained")
    file(MAKE_DIRECTORY "${CAORS_GENERATED_HEADER_TEST_DIR}")

    foreach(CAORS_HEADER IN LISTS CAORS_HEADER_SELF_CONTAINED_HEADERS)
      file(RELATIVE_PATH CAORS_HEADER_RELATIVE "${CAORS_ROOT_DIR}/include" "${CAORS_HEADER}")
      string(REPLACE "\\" "/" CAORS_HEADER_INCLUDE "${CAORS_HEADER_RELATIVE}")
      if(CAORS_HEADER_INCLUDE MATCHES "/detail/")
        continue()
      endif()

      string(MAKE_C_IDENTIFIER "${CAORS_HEADER_INCLUDE}" CAORS_HEADER_IDENTIFIER)
      set(CAORS_GENERATED_HEADER_TEST_SOURCE
          "${CAORS_GENERATED_HEADER_TEST_DIR}/${CAORS_HEADER_IDENTIFIER}.cpp")
      file(
        WRITE "${CAORS_GENERATED_HEADER_TEST_SOURCE}"
        "// 此文件由 tests/CMakeLists.txt 生成，用于验证头文件可单独包含。\n"
        "#include <${CAORS_HEADER_INCLUDE}>\n\n"
        "namespace {\n"
        "[[maybe_unused]] constexpr int caors_header_self_contained_check = 0;\n"
        "} // namespace\n"
      )
      list(APPEND GENERATED_TEST_SOURCES "${CAORS_GENERATED_HEADER_TEST_SOURCE}")
    endforeach()
  endif()

  if(NOT TEST_SOURCES AND NOT GENERATED_TEST_SOURCES)
    return()
  endif()

  string(MAKE_C_IDENTIFIER "${TEST_DIRECTORY}" TEST_IDENTIFIER)
  set(TARGET_NAME "caors_${TEST_IDENTIFIER}_test")

  add_executable(${TARGET_NAME})
  target_sources(${TARGET_NAME} PRIVATE ${TEST_SOURCES} ${GENERATED_TEST_SOURCES})
  target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
  target_compile_warnings(${TARGET_NAME})
  target_include_directories(${TARGET_NAME} PRIVATE "${TEST_SOURCE_DIR}")

  if(TEST_DIRECTORY MATCHES "^serialization/")
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/serialization")
  endif()

  target_link_libraries(${TARGET_NAME} PRIVATE caors::caors)
  target_link_libraries(${TARGET_NAME} PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

  if(TEST_SOURCES)
    source_group(TREE "${TEST_SOURCE_DIR}" FILES ${TEST_SOURCES})
  endif()
  if(GENERATED_TEST_SOURCES)
    source_group("Generated" FILES ${GENERATED_TEST_SOURCES})
  endif()
  set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Tests")

  string(REPLACE "/" "." TEST_NAME "${TEST_DIRECTORY}")
  add_test(NAME "caors.${TEST_NAME}" COMMAND ${TARGET_NAME})
endfunction()

function(caors_add_serialization_test_directories)
  set(SERIALIZATION_TEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/serialization")
  file(GLOB SERIALIZATION_TEST_DIRECTORIES CONFIGURE_DEPENDS RELATIVE "${SERIALIZATION_TEST_ROOT}"
       "${SERIALIZATION_TEST_ROOT}/*")
  list(SORT SERIALIZATION_TEST_DIRECTORIES)

  foreach(SERIALIZATION_TEST_DIRECTORY IN LISTS SERIALIZATION_TEST_DIRECTORIES)
    if(IS_DIRECTORY "${SERIALIZATION_TEST_ROOT}/${SERIALIZATION_TEST_DIRECTORY}")
      caors_add_test_directory("serialization/${SERIALIZATION_TEST_DIRECTORY}")
    endif()
  endforeach()
endfunction()

file(GLOB CAORS_TEST_DIRECTORIES CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*")
list(SORT CAORS_TEST_DIRECTORIES)

foreach(CAORS_TEST_DIRECTORY IN LISTS CAORS_TEST_DIRECTORIES)
  if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${CAORS_TEST_DIRECTORY}")
    if(CAORS_TEST_DIRECTORY STREQUAL "serialization")
      caors_add_serialization_test_directories()
    else()
      caors_add_test_directory("${CAORS_TEST_DIRECTORY}")
    endif()
  endif()
endforeach()
