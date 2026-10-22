// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_OPTIONS_H_
#define CAORS_IO_OPTIONS_H_
#include <cstdint>
namespace caors::io {
/** @brief 当前仅提供只读访问；写入模式随实现开放，不伪造成功。 */
enum class file_access { read };
/** @brief 当前仅打开已有文件，绝不隐式创建或截断。 */
enum class file_creation { open_existing };
/** @brief 只读打开与单次范围读取预算；默认不创建、不截断文件。 */
struct open_options {
  file_access access = file_access::read;
  file_creation creation = file_creation::open_existing;
  std::uint64_t max_read_bytes = 64 * 1024 * 1024;
};
/** @brief 整体结果预算，读取前检查，不是全局在途预算。 */
struct read_options {
  std::uint64_t max_bytes = 64 * 1024 * 1024;
};
} // namespace caors::io
#endif
