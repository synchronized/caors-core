// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_NATIVE_FILE_SYSTEM_H_
#define CAORS_IO_NATIVE_FILE_SYSTEM_H_

#include "caors/io/file_system.h"

namespace caors::io {

/** @brief 基于宿主操作系统文件系统的同步实现。 */
class CAORS_API native_file_system final : public file_system {
public:
  using file_system::write_all;

  [[nodiscard]] bool exists(path_view path) const override;
  [[nodiscard]] byte_buffer_result read_all(path_view path,
                                            memory_resource* resource = nullptr) override;
  [[nodiscard]] error write_all(path_view path, std::span<const std::byte> bytes) override;
};

/** @brief 获取进程内默认本地文件系统实例。 */
[[nodiscard]] CAORS_API native_file_system& get_native_file_system() noexcept;

} // namespace caors::io

namespace caors {

using io::get_native_file_system;
using io::native_file_system;

} // namespace caors

#endif
