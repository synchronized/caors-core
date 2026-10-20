# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors
# Copyright (c) 2026 Gneiss contributors
# Copyright (c) 2026 Granit contributors

include_guard(GLOBAL)

set(CAORS_DEPENDENCY_SOURCE_CACHE_DIR "${CAORS_ROOT_DIR}/vendor/cache" CACHE PATH
    "第三方归档及只读源码缓存，不包含编译产物")
option(CAORS_DEPENDENCY_OFFLINE "依赖只使用已有缓存，不访问网络" OFF)
set(CAORS_DEPENDENCY_LOCK_TIMEOUT "300" CACHE STRING "等待单项依赖缓存锁的秒数")

# 锁覆盖检查到发布全过程；失败保留现有项及本次 staging，避免误删用户数据。
function(caors_prepare_source_cache name url sha256 marker output)
  string(LENGTH "${sha256}" hash_length)
  if(NOT name MATCHES "^[A-Za-z0-9_-]+$" OR NOT hash_length EQUAL 64 OR
     NOT sha256 MATCHES "^[0-9a-f]+$" OR NOT output OR
     NOT marker MATCHES "^[A-Za-z0-9_.+-]+(/[A-Za-z0-9_.+-]+)*$" OR
     marker MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR "依赖缓存参数无效：${name}")
  endif()
  if(NOT CAORS_DEPENDENCY_LOCK_TIMEOUT MATCHES "^[0-9]+$")
    message(FATAL_ERROR "CAORS_DEPENDENCY_LOCK_TIMEOUT 必须为非负整数")
  endif()
  get_filename_component(root "${CAORS_DEPENDENCY_SOURCE_CACHE_DIR}" ABSOLUTE
                         BASE_DIR "${CAORS_ROOT_DIR}")
  file(MAKE_DIRECTORY "${root}/locks" "${root}/archives" "${root}/staging" "${root}/sources/${name}")
  file(REAL_PATH "${root}" root)
  set(key "${name}-${sha256}")
  set(source "${root}/sources/${name}/${sha256}")
  set(archive "${root}/archives/${key}.tar.gz")
  file(LOCK "${root}/locks/${key}.lock" GUARD FUNCTION
       TIMEOUT "${CAORS_DEPENDENCY_LOCK_TIMEOUT}" RESULT_VARIABLE locked)
  if(NOT locked STREQUAL "0")
    message(FATAL_ERROR "依赖 ${name} 缓存锁失败：${root}：${locked}")
  endif()
  if(EXISTS "${source}/.caors-source-complete" AND EXISTS "${source}/${marker}" AND
     NOT IS_DIRECTORY "${source}/${marker}")
    file(READ "${source}/.caors-source-complete" receipt)
    if(receipt STREQUAL "${sha256}\n")
      set(${output} "${source}" PARENT_SCOPE)
      return()
    endif()
  endif()
  if(EXISTS "${source}")
    message(FATAL_ERROR "依赖 ${name} 源码缓存不完整，请移走后重试：${source}")
  endif()
  if(EXISTS "${archive}")
    file(SHA256 "${archive}" actual)
    if(NOT actual STREQUAL "${sha256}")
      message(FATAL_ERROR "依赖 ${name} 归档摘要不匹配，请移走后重试：${archive}")
    endif()
  elseif(CAORS_DEPENDENCY_OFFLINE)
    message(FATAL_ERROR "离线依赖 ${name} 缓存缺失：${root}")
  endif()

  string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef nonce)
  string(SUBSTRING "${sha256}" 0 12 short_hash)
  # staging 不重复完整摘要，避免 Windows 子进程工作目录触及长路径限制。
  set(stage "${root}/staging/${name}-${short_hash}-${nonce}")
  if(EXISTS "${stage}")
    message(FATAL_ERROR "依赖 staging 路径已存在，不覆盖：${stage}")
  endif()
  file(MAKE_DIRECTORY "${stage}/unpack")
  if(NOT EXISTS "${archive}")
    message(STATUS "下载锁定依赖 ${name}：${root}")
    file(DOWNLOAD "${url}" "${stage}/download.tar.gz" TLS_VERIFY ON
         TIMEOUT 300 INACTIVITY_TIMEOUT 30 STATUS download_status)
    list(GET download_status 0 code)
    if(NOT code EQUAL 0)
      message(FATAL_ERROR "依赖 ${name} 下载失败：${download_status}；暂存：${stage}")
    endif()
    file(SHA256 "${stage}/download.tar.gz" actual)
    if(NOT actual STREQUAL "${sha256}")
      message(FATAL_ERROR "依赖 ${name} 下载摘要不匹配：${stage}")
    endif()
    file(RENAME "${stage}/download.tar.gz" "${archive}" RESULT renamed NO_REPLACE)
    if(NOT renamed STREQUAL "0")
      message(FATAL_ERROR "依赖 ${name} 归档发布失败：${renamed}；暂存：${stage}")
    endif()
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xzf "${archive}"
    WORKING_DIRECTORY "${stage}/unpack" RESULT_VARIABLE extracted ERROR_VARIABLE extract_error)
  if(NOT extracted STREQUAL "0")
    message(FATAL_ERROR "依赖 ${name} 解压失败（${extracted}）：${extract_error}；暂存：${stage}")
  endif()
  file(GLOB entries LIST_DIRECTORIES TRUE "${stage}/unpack/*")
  list(LENGTH entries count)
  if(NOT count EQUAL 1)
    message(FATAL_ERROR "依赖 ${name} 归档必须有单一根目录：${stage}")
  endif()
  list(GET entries 0 unpacked)
  if(NOT IS_DIRECTORY "${unpacked}" OR NOT EXISTS "${unpacked}/${marker}" OR
     IS_DIRECTORY "${unpacked}/${marker}")
    message(FATAL_ERROR "依赖 ${name} 缺少入口 ${marker}：${stage}")
  endif()
  file(CONFIGURE OUTPUT "${unpacked}/.caors-source-complete"
       CONTENT "${sha256}\n" @ONLY NEWLINE_STYLE UNIX)
  # NO_REPLACE 在本次 Linux CMake 环境不能用于目录；
  # 同项锁下再次检查目标，再以普通 rename 原子发布目录。
  if(EXISTS "${source}")
    message(FATAL_ERROR "依赖 ${name} 源码发布目标已存在，不覆盖：${source}")
  endif()
  file(RENAME "${unpacked}" "${source}" RESULT published)
  if(NOT published STREQUAL "0")
    message(FATAL_ERROR "依赖 ${name} 源码发布失败：${published}；暂存：${stage}")
  endif()

  # 只清理本次创建且解析后仍位于缓存 staging 内的目录。
  file(REAL_PATH "${root}/staging" staging_root)
  file(REAL_PATH "${stage}" stage_real)
  cmake_path(IS_PREFIX staging_root "${stage_real}" NORMALIZE within_staging)
  if(NOT within_staging OR stage_real STREQUAL staging_root)
    message(FATAL_ERROR "拒绝清理缓存 staging 之外的路径：${stage_real}")
  endif()
  file(REMOVE_RECURSE "${stage_real}")
  set(${output} "${source}" PARENT_SCOPE)
endfunction()
