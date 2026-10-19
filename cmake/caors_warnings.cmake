# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

# 编译警告默认只作用于本项目目标，不传递给下游使用者。
option(CAORS_ENABLE_WARNINGS "启用 CAORS 目标编译警告" ON)
option(CAORS_ENABLE_PEDANTIC_WARNINGS "启用 CAORS 目标严格标准扩展警告" OFF)
option(CAORS_WARNINGS_AS_ERRORS "将 CAORS 目标编译警告视为错误" OFF)

function(caors_target_compile_warnings target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "目标不存在: ${target}")
  endif()

  if(NOT CAORS_ENABLE_WARNINGS)
    return()
  endif()

  target_compile_options(
    ${target}
    PRIVATE
      $<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4>
      $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4>
      $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall>
      $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall>
      $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wextra>
      $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wextra>
  )

  if(CAORS_ENABLE_PEDANTIC_WARNINGS)
    target_compile_options(
      ${target}
      PRIVATE
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wpedantic>
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wpedantic>
    )
  endif()

  if(NOT CAORS_WARNINGS_AS_ERRORS)
    return()
  endif()

  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    set_property(TARGET ${target} PROPERTY COMPILE_WARNING_AS_ERROR ON)
    return()
  endif()

  target_compile_options(
    ${target}
    PRIVATE
      $<$<COMPILE_LANG_AND_ID:C,MSVC>:/WX>
      $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX>
      $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Werror>
      $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Werror>
  )
endfunction()
