# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

# 工程版本的唯一来源，其余版本常量和包元数据由 project() 派生。
set(CAORS_PROJECT_VERSION "0.5.0")

if(NOT CAORS_PROJECT_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "无效的 CAORS 工程版本：${CAORS_PROJECT_VERSION}")
endif()
