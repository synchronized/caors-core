// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_REGISTRY_H_
#define CAORS_SERIALIZATION_REGISTRY_H_

#include "caors/serialization/types.h"

#include <cstddef>

namespace caors::serialization {

/** @brief 查找指定元类型的序列化 schema 视图，不复制 schema 数据。 */
[[nodiscard]] CAORS_API schema_view find_schema(meta::type type) noexcept;

/** @brief 判断指定元类型是否挂载了序列化 extension。 */
[[nodiscard]] CAORS_API bool contains(meta::type type);

/** @brief 获取当前序列化 extension 数量。 */
[[nodiscard]] CAORS_API std::size_t size() noexcept;

} // namespace caors::serialization

#endif
