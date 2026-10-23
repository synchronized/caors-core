// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_DETAIL_READ_OPERATION_H_
#define CAORS_IO_DETAIL_READ_OPERATION_H_
#include "caors/io/async_file.h"
#include "caors/io/backend.h"
namespace caors::io::detail {
/** @brief 新后端整体读取，与兼容入口复用同一个关闭及取消状态机。 */
[[nodiscard]] CAORS_API io_error start_read_file(backend_location source, tasks::executor executor,
                                                 std::uint64_t max_bytes, memory_resource* resource,
                                                 io_completion<byte_buffer> completion);
} // namespace caors::io::detail
#endif
