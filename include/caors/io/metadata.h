// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_METADATA_H_
#define CAORS_IO_METADATA_H_
#include "caors/io/types.h"
#include <chrono>
#include <cstdint>

namespace caors::io {
/** @brief 文件系统条目类型。 */
enum class file_kind {
  /** @brief 普通文件或可读取字节对象。 */
  file,
  /** @brief 目录或虚拟目录。 */
  directory,
};

/** @brief 目录枚举返回的一条直接子项。 */
struct directory_entry {
  /** @brief 直接子项名称，不包含父目录路径。 */
  caors::io::path name;
  /** @brief 所属访问入口的路径；io_context 会映射回逻辑目录。 */
  caors::io::path path;
  /** @brief 子项类型。 */
  file_kind kind = file_kind::file;
};

/** @brief 目录项列表，使用 CAORS allocator 管理存储。 */
using directory_entry_list = vector<directory_entry>;

/** @brief 目录枚举结果。 */
using directory_entry_list_result = result<directory_entry_list>;

/** @brief 文件或目录的基础属性。 */
struct file_status {
  /** @brief 文件或目录类型。 */
  file_kind kind = file_kind::file;
  /** @brief 字节大小；只有 `has_size == true` 时有效。 */
  std::uint64_t size = 0;
  /** @brief 是否提供了 `size`。目录或虚拟目录通常不提供。 */
  bool has_size = false;
  /** @brief 最后修改时间；只有 `has_modified_time == true` 时有效。 */
  std::chrono::system_clock::time_point modified_time{};
  /** @brief 是否提供了 `modified_time`。 */
  bool has_modified_time = false;
  /** @brief 是否只读；只有 `has_read_only == true` 时有效。 */
  bool read_only = false;
  /** @brief 是否提供了 `read_only`。 */
  bool has_read_only = false;
};

/** @brief 文件或目录状态查询结果。 */
using file_status_result = result<file_status>;

} // namespace caors::io
#endif
