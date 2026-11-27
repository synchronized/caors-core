// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_SERIALIZATION_BYTES_H_
#define CAORS_SERIALIZATION_BYTES_H_
#include "caors/core/containers.h"
#include "caors/result.h"
#include <cstddef>
namespace caors::serialization {
/** @brief 序列化编码的拥有式字节结果；不包含文件或流语义。 */
using byte_buffer = core::vector<std::byte>;
/** @brief 编码操作返回的字节结果。 */
using byte_buffer_result = result<byte_buffer>;
/** @brief 使用指定分配器构造编码缓冲区。 */
[[nodiscard]] inline byte_buffer make_byte_buffer(memory_resource* resource = nullptr) {
  return byte_buffer{allocator<std::byte>{resource}};
}
}
#endif
