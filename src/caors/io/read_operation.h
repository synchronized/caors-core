// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_DETAIL_READ_OPERATION_H_
#define CAORS_IO_DETAIL_READ_OPERATION_H_
#include "caors/io/async_file.h"
#include "caors/io/route_snapshot.h"
namespace caors::io::detail {
/** @brief 新调用方与旧资源适配共用的打开/读取/关闭编排；worker_mode 仅供旧后端显式适配。 */
[[nodiscard]] CAORS_API io_error start_read_file(resolved_file source, tasks::executor executor,
                                                 std::uint64_t max_bytes, memory_resource* resource,
                                                 io_completion<byte_buffer> completion,
                                                 bool worker_mode = false);
} // namespace caors::io::detail
#endif
