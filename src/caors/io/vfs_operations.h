// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_DETAIL_VFS_OPERATIONS_H_
#define CAORS_IO_DETAIL_VFS_OPERATIONS_H_
#include "caors/io/backend.h"
#include "caors/io/file_service.h"
namespace caors::io::detail {
// 资源边界可解析固定快照，普通调用仍由 file_service 封装。
class vfs_access {
public:
  static result<backend_location> resolve(const file_batch& snapshot, path_view path) {
    return snapshot.resolve(path);
  }
};
[[nodiscard]] CAORS_API io_error start_write(backend_location source, byte_buffer bytes,
                                             io_completion<std::uint64_t> completion);
[[nodiscard]] CAORS_API io_error start_stat(backend_location source,
                                            io_completion<file_status> completion);
[[nodiscard]] CAORS_API io_error start_list(backend_location source, memory_resource* resource,
                                            io_completion<directory_entry_list> completion);
}
#endif
