// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_TYPES_H_
#define CAORS_IO_TYPES_H_

#include "caors/core/containers.h"
#include "caors/io/path.h"
#include "caors/result.h"

#include <cstddef>

namespace caors::io {

/** @brief 字节缓冲区，使用 CAORS allocator 管理存储。 */
using byte_buffer = vector<std::byte>;

/** @brief 字节缓冲区读取结果。 */
using byte_buffer_result = result<byte_buffer>;

/** @brief 字节数量结果。 */
using size_result = result<std::size_t>;

/** @brief 使用指定内存资源创建字节缓冲区。 */
[[nodiscard]] inline byte_buffer make_byte_buffer(memory_resource* resource = nullptr) {
  return byte_buffer{allocator<std::byte>{resource}};
}

} // namespace caors::io

namespace caors {

using io::byte_buffer;
using io::byte_buffer_result;
using io::make_byte_buffer;
using io::size_result;

} // namespace caors

#endif
