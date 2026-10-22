// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_DETAIL_VFS_OPERATIONS_H_
#define CAORS_IO_DETAIL_VFS_OPERATIONS_H_
#include "caors/io/backend.h"
#include "caors/io/route_snapshot.h"
namespace caors::io::detail {
[[nodiscard]] CAORS_API io_error start_write(resolved_file source, byte_buffer bytes,
                                             io_completion<std::uint64_t> completion);
[[nodiscard]] CAORS_API io_error start_stat(resolved_file source,
                                            io_completion<file_status> completion);
[[nodiscard]] CAORS_API io_error start_list(resolved_file source, memory_resource* resource,
                                            io_completion<directory_entry_list> completion);
}
#endif
