// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_CREATION_H_
#define CAORS_META_TYPE_CREATION_H_

#include "caors/memory/resource.h"
#include "caors/meta/argument.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any.h"

/**
 * @file
 * @brief `meta::type` 运行期值创建 API 的完整依赖头。
 *
 * `caors/meta/type.h` 只声明 `meta::type`，为了保持类型查询头轻量，不强制引入
 * `meta::any` 的完整定义。调用 `meta::type::create()`、`create_value()` 或处理返回的
 * `meta::any` 时，应包含本头或 `caors/meta.h`。
 */

#endif
