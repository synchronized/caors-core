# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

add_executable(caors_tasks_asio_config_test asio_config_test.cpp)
target_compile_features(caors_tasks_asio_config_test PRIVATE cxx_std_20)
caors_target_compile_warnings(caors_tasks_asio_config_test)
target_link_libraries(
  caors_tasks_asio_config_test
  PRIVATE caors::caors caors_asio_config Catch2::Catch2 Catch2::Catch2WithMain
)
set_target_properties(caors_tasks_asio_config_test PROPERTIES FOLDER "Tests")

add_test(NAME caors.tasks.asio_config COMMAND caors_tasks_asio_config_test)
if(BUILD_SHARED_LIBS)
  set_property(
    TEST caors.tasks.asio_config
    PROPERTY ENVIRONMENT_MODIFICATION
             "PATH=path_list_prepend:$<TARGET_FILE_DIR:caors>"
  )
endif()
