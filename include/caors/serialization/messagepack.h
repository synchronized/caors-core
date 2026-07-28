// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_MESSAGEPACK_H_
#define CAORS_SERIALIZATION_MESSAGEPACK_H_

#include "caors/io/types.h"
#include "caors/serialization/types.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace caors::serialization {

/** @brief MessagePack DOM 解析限制。 */
struct messagepack_options {
  /** @brief 允许的最大容器嵌套深度；0 表示不限制。 */
  std::uint32_t max_parse_depth = 64;
  /** @brief 单个字符串允许的最大字节数；0 表示不限制。 */
  std::size_t max_string_size = 64U * 1024U * 1024U;
  /** @brief 单个数组或 map 允许的最大元素数量；0 表示不限制。 */
  std::size_t max_container_size = 16U * 1024U * 1024U;
};

/** @brief 将 DOM 值编码为标准 MessagePack 字节。 */
[[nodiscard]] CAORS_API io::byte_buffer_result
encode_messagepack(const variant& value, memory_resource* resource = nullptr);

/**
 * @brief 从一个完整 MessagePack 根值解析 DOM。
 *
 * 字符串 key 的 map 解析为 object，其他 map 解析为 dictionary。bin、ext、保留标记和根值后的
 * 尾随数据在当前版本中返回 `errc::unsupported` 或 `errc::invalid_argument`。
 */
[[nodiscard]] CAORS_API variant_result parse_messagepack(std::span<const std::byte> bytes,
                                                         messagepack_options options = {});

} // namespace caors::serialization

#endif
