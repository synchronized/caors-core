// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_MOUNT_FILE_SYSTEM_H_
#define CAORS_IO_MOUNT_FILE_SYSTEM_H_

#include "caors/io/file_system.h"

#include <cstddef>
#include <string_view>

namespace caors::io {

/**
 * @brief 基于虚拟路径前缀转发到其他文件系统的同步文件系统。
 *
 * `mount_file_system` 不拥有被挂载的 `file_system`，调用方需要保证挂载目标在取消挂载前保持有效。
 * 虚拟路径使用 `/` 作为分隔符；目标路径语义仍由具体目标文件系统解释。
 */
class CAORS_API mount_file_system final : public file_system {
public:
  using file_system::write_all;

  explicit mount_file_system(memory_resource* resource = nullptr);

  /** @brief 返回内部挂载表使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept;

  /** @brief 挂载虚拟路径前缀到目标文件系统和目标路径前缀。 */
  [[nodiscard]] error mount(path_view prefix, file_system& target, path_view target_prefix = {});

  /** @brief 取消挂载虚拟路径前缀。 */
  [[nodiscard]] bool unmount(path_view prefix);

  /** @brief 清空所有挂载。 */
  void clear();

  /** @brief 返回挂载数量。 */
  [[nodiscard]] std::size_t mount_count() const noexcept;

  [[nodiscard]] bool exists(path_view path) const override;
  [[nodiscard]] byte_buffer_result read_all(path_view path,
                                            memory_resource* resource = nullptr) override;
  [[nodiscard]] error write_all(path_view path, std::span<const std::byte> bytes) override;

private:
  struct mount_entry {
    core::string prefix;
    file_system* target = nullptr;
    core::string target_prefix;
  };

  [[nodiscard]] const mount_entry* find_mount(path_view path) const noexcept;
  [[nodiscard]] core::string resolve_path(const mount_entry& entry, path_view path) const;

  core::vector<mount_entry> mounts_;
};

} // namespace caors::io

namespace caors {

using io::mount_file_system;

} // namespace caors

#endif
