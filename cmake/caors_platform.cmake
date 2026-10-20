# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

include_guard(GLOBAL)
if(EMSCRIPTEN)
  if(CAORS_BUILD_SHARED)
    message(FATAL_ERROR "Emscripten 基线仅支持静态库，请设置 CAORS_LIBRARY_TYPE=STATIC")
  endif()
  include(CheckCXXSourceCompiles)
  # 编译选项可能在同一构建目录变更，不能沿用旧的能力缓存。
  unset(CAORS_WEB_PTHREADS_ENABLED CACHE)
  check_cxx_source_compiles("#ifndef __EMSCRIPTEN_PTHREADS__
#error single_thread
#endif
int main() {}" CAORS_WEB_PTHREADS_ENABLED)
  if(CAORS_WEB_PTHREADS_ENABLED)
    message(FATAL_ERROR "Emscripten 首版仅验证无 pthread 构建，请移除 -pthread")
  endif()
endif()
