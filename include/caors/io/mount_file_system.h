// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_MOUNT_FILE_SYSTEM_H_
#define CAORS_IO_MOUNT_FILE_SYSTEM_H_

#include "caors/io/file_system.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <string_view>

namespace caors::io {

/**
 * @brief 基于虚拟路径前缀转发到其他文件系统的同步文件系统。
 *
 * 借用挂载要求调用方保证后端寿命；拥有式挂载持有后端及可选宿主寿命凭据。
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

  /**
   * @brief 挂载拥有式后端；空 target 返回 invalid_argument，重复前缀不替换旧挂载。
   * @param lifetime 可选宿主寿命凭据，例如模块 pin；其释放晚于本挂载持有的后端所有权。
   * @note 调用方仍须保证路由器 allocator 的寿命，独立持有 target 时须自行延长相关模块寿命。
   * 挂载更新与查询可并发；后端自身的线程约束仍由调用方遵守。分配失败可抛出 bad_alloc。
   */
  [[nodiscard]] error mount(path_view prefix, std::shared_ptr<file_system> target,
                            path_view target_prefix = {}, std::shared_ptr<void> lifetime = {});

  /** @brief 取消挂载虚拟路径前缀。 */
  [[nodiscard]] bool unmount(path_view prefix);

  /** @brief 清空所有挂载。 */
  void clear();

  /** @brief 返回挂载数量。 */
  [[nodiscard]] std::size_t mount_count() const noexcept;

  [[nodiscard]] bool exists(path_view path) const override;
  [[nodiscard]] file_system_capabilities capabilities() const noexcept override;
  [[nodiscard]] file_status_result status(path_view path) const override;
  [[nodiscard]] directory_entry_list_result
  list(path_view directory, memory_resource* result_resource = nullptr) override;
  [[nodiscard]] byte_buffer_result read_all(path_view path,
                                            memory_resource* result_resource = nullptr) override;
  [[nodiscard]] error write_all(path_view path, std::span<const std::byte> bytes) override;

  [[nodiscard]] bool routes_paths() const noexcept override { return true; }
  [[nodiscard]] result<resolved_file> resolve_owned(path_view location) const override;
  [[nodiscard]] result<std::shared_ptr<file_system>>
  freeze_routes(detail::snapshot_context& context, std::shared_ptr<void> owner) const override;

private:
  struct mount_entry {
    std::shared_ptr<void> owner;
    std::uint64_t source_id = 0;
    core::string prefix;
    file_system* target = nullptr;
    core::string target_prefix;
  };

  [[nodiscard]] const mount_entry* find_mount(const core::vector<mount_entry>& entries,
                                              path_view path) const noexcept;
  [[nodiscard]] core::string resolve_path(const mount_entry& entry, path_view path) const;

  [[nodiscard]] error mount_owned(path_view prefix, file_system& target, path_view target_prefix,
                                  std::shared_ptr<void> owner);
  using route_table = core::vector<mount_entry>;
  memory_resource* resource_;
  std::atomic<std::shared_ptr<const route_table>> mounts_;
};

} // namespace caors::io

#endif
