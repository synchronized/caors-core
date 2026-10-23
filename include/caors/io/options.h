// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_OPTIONS_H_
#define CAORS_IO_OPTIONS_H_
#include <cstdint>
namespace caors::io {
/** @brief 文件访问权限；默认只读，后端仍可能拒绝未支持的模式。 */
enum class file_access { read, write, read_write };
/** @brief 创建策略；默认不创建或截断，create_always 必须配合可写访问。 */
enum class file_creation { open_existing, create_always };
/** @brief 文件权限、创建策略与单次操作预算；默认只读已有文件，不提供 append 隐式游标。 */
struct open_options {
  file_access access = file_access::read;
  file_creation creation = file_creation::open_existing;
  std::uint64_t max_read_bytes = 64 * 1024 * 1024;
  std::uint64_t max_write_bytes = 64 * 1024 * 1024;
};
/** @brief 整体结果预算，读取前检查，不是全局在途预算。 */
struct read_options {
  std::uint64_t max_bytes = 64 * 1024 * 1024;
};
} // namespace caors::io
#endif
