# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

set(TARGET_NAME caors_config_test)
add_executable(${TARGET_NAME})

file(GLOB_RECURSE LIB_SOURCE CONFIGURE_DEPENDS "*.cpp" "*.hpp" "*.h")
target_sources(${TARGET_NAME} PRIVATE ${LIB_SOURCE})

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
target_compile_warnings(${TARGET_NAME})

target_link_libraries(${TARGET_NAME} PRIVATE caors::caors)
target_link_libraries(${TARGET_NAME} PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Tests")

add_test(NAME caors.config COMMAND ${TARGET_NAME})
