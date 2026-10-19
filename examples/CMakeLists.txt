# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

function(caors_add_example target)
  add_executable(${target})
  target_sources(${target} PRIVATE ${ARGN})
  target_compile_features(${target} PRIVATE cxx_std_20)
  caors_target_compile_warnings(${target})
  target_link_libraries(${target} PRIVATE caors::caors)
  set_target_properties(${target} PROPERTIES FOLDER "Examples")
  set_property(GLOBAL APPEND PROPERTY CAORS_EXAMPLE_TARGETS ${target})
endfunction()

file(GLOB CAORS_EXAMPLE_DIRECTORIES CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*")
list(SORT CAORS_EXAMPLE_DIRECTORIES)

foreach(CAORS_EXAMPLE_DIRECTORY IN LISTS CAORS_EXAMPLE_DIRECTORIES)
  if(
    IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${CAORS_EXAMPLE_DIRECTORY}"
    AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${CAORS_EXAMPLE_DIRECTORY}/CMakeLists.txt"
  )
    add_subdirectory("${CAORS_EXAMPLE_DIRECTORY}")
  endif()
endforeach()

get_property(CAORS_EXAMPLE_TARGETS GLOBAL PROPERTY CAORS_EXAMPLE_TARGETS)
if(CAORS_EXAMPLE_TARGETS)
  add_custom_target(caors_examples_check DEPENDS ${CAORS_EXAMPLE_TARGETS})
  set_target_properties(caors_examples_check PROPERTIES FOLDER "Examples")
endif()
