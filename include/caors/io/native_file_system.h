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

  explicit native_file_system(memory_resource* resource = nullptr);
  explicit native_file_system(path_view root, memory_resource* resource = nullptr);

  /** @brief 返回 root 路径存储使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept;

  /** @brief 返回当前本地文件系统根目录。空 root 表示直接使用传入路径。 */
  [[nodiscard]] path_view root() const noexcept;

  /** @brief 设置本地文件系统根目录。root 不会按虚拟路径规则规范化。 */
  void set_root(path_view root);

  [[nodiscard]] bool exists(path_view path) const override;
  [[nodiscard]] directory_entry_list_result list(path_view directory,
                                                 memory_resource* resource = nullptr) override;
  [[nodiscard]] byte_buffer_result read_all(path_view path,
                                            memory_resource* resource = nullptr) override;
  [[nodiscard]] input_stream_result open_read(path_view path,
                                              memory_resource* resource = nullptr) override;
  [[nodiscard]] output_stream_result open_write(path_view path,
                                                memory_resource* resource = nullptr) override;
  [[nodiscard]] error write_all(path_view path, std::span<const std::byte> bytes) override;

private:
  core::string root_;
};

/** @brief 获取进程内默认本地文件系统实例。 */
[[nodiscard]] CAORS_API native_file_system& get_native_file_system() noexcept;

} // namespace caors::io

#endif
