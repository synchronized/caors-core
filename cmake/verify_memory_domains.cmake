# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

function(caors_verify_memory_domains)
  file(
    GLOB_RECURSE memory_domain_sources
    CONFIGURE_DEPENDS
    "${CAORS_ROOT_DIR}/include/caors/*.h"
    "${CAORS_ROOT_DIR}/include/caors/*.hpp"
    "${CAORS_ROOT_DIR}/src/caors/*.cpp"
  )

  set(legacy_resource_api
      "${CAORS_ROOT_DIR}/include/caors/memory/resource.h"
      "${CAORS_ROOT_DIR}/src/caors/memory/resource.cpp")
  foreach(source IN LISTS memory_domain_sources)
    if(source IN_LIST legacy_resource_api)
      continue()
    endif()

    file(READ "${source}" contents)
    if(contents MATCHES "library_resource[ \t\r\n]*\\(")
      message(
        FATAL_ERROR
        "${source} 调用了兼容入口 library_resource()；请按生命周期改用 process、registry、scratch 或 result resource"
      )
    endif()
  endforeach()
endfunction()
