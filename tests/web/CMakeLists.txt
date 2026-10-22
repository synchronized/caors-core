# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

if(NOT EMSCRIPTEN)
  return()
endif()
add_executable(caors_web_protocol_test
  ../assets/asset_manager/asynchronous_loader_test.cpp
  ../assets/asset_manager/vfs_source_test.cpp
  ../core/shared_snapshot/shared_snapshot_test.cpp
  ../version/version_test.cpp
  ../resources/provider/byte_reader_test.cpp
  ../io/vfs_context_test.cpp
  ../io/vfs_write_test.cpp
  ../io/async_file_test.cpp
  ../io/async_request_test.cpp
  ../io/chunked_file_test.cpp
  ../io/route_snapshot_test.cpp
  platform_test.cpp)
target_link_libraries(caors_web_protocol_test PRIVATE caors::caors Catch2::Catch2WithMain)
caors_target_compile_warnings(caors_web_protocol_test)
add_test(NAME caors.web.protocol COMMAND caors_web_protocol_test)
set_tests_properties(caors.web.protocol PROPERTIES TIMEOUT 90)

add_executable(caors_web_browser_smoke browser_smoke.cpp)
target_link_libraries(caors_web_browser_smoke PRIVATE caors::caors)
caors_target_compile_warnings(caors_web_browser_smoke)
target_link_options(caors_web_browser_smoke PRIVATE
  "-sENVIRONMENT=web" "-sALLOW_MEMORY_GROWTH=1" "-sNO_EXIT_RUNTIME=1")
set_target_properties(caors_web_browser_smoke PROPERTIES SUFFIX ".js")
configure_file(browser.html browser.html COPYONLY)
