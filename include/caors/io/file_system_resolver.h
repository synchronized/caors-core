// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_FILE_SYSTEM_RESOLVER_H_
#define CAORS_IO_FILE_SYSTEM_RESOLVER_H_

#include "caors/io/file_system.h"
#include "caors/io/uri.h"

#include "caors/core/detail/shared_snapshot.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <span>

namespace caors::io {

/**
 * @brief 基于 URI scheme / authority / 路径前缀的文件系统路由器。
 *
 * `file_system_resolver` 本身也是一个 `file_system`，调用方可以把 `caors://assets/a.png`、
 * `file://project/config.json` 或普通虚拟路径交给它，由 resolver 转发到已注册的后端文件系统。
 * 借用挂载不拥有后端；拥有式挂载持有后端及可选宿主寿命凭据。
 */
class CAORS_API file_system_resolver final : public file_system {
public:
  using file_system::write_all;

  explicit file_system_resolver(memory_resource* resource = nullptr);

  /** @brief 返回内部路由表使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept;

  /** @brief 挂载 URI 前缀到目标文件系统和目标路径前缀。 */
  [[nodiscard]] error mount(uri_view prefix, file_system& target, path_view target_prefix = {});

  /**
   * @brief 挂载拥有式后端；空 target 返回 invalid_argument，重复前缀不替换旧挂载。
   * @param lifetime 可选宿主寿命凭据，例如模块 pin；其释放晚于本挂载持有的后端所有权。
   * @note 调用方仍须保证路由器 allocator 的寿命，独立持有 target 时须自行延长相关模块寿命。
   * 挂载更新与查询可并发；后端自身的线程约束仍由调用方遵守。分配失败可抛出 bad_alloc。
   */
  [[nodiscard]] error mount(uri_view prefix, std::shared_ptr<file_system> target,
                            path_view target_prefix = {}, std::shared_ptr<void> lifetime = {});

  /** @brief 取消挂载指定 URI 前缀。 */
  [[nodiscard]] bool unmount(uri_view prefix);

  /** @brief 清空全部路由。 */
  void clear();

  /** @brief 返回路由数量。 */
  [[nodiscard]] std::size_t mount_count() const noexcept;

  [[nodiscard]] bool exists(path_view location) const override;
  [[nodiscard]] file_system_capabilities capabilities() const noexcept override;
  [[nodiscard]] file_status_result status(path_view location) const override;
  [[nodiscard]] directory_entry_list_result
  list(path_view directory, memory_resource* result_resource = nullptr) override;
  [[nodiscard]] byte_buffer_result read_all(path_view location,
                                            memory_resource* result_resource = nullptr) override;
  [[nodiscard]] input_stream_result open_read(path_view location,
                                              memory_resource* result_resource = nullptr) override;
  [[nodiscard]] output_stream_result
  open_write(path_view location, memory_resource* result_resource = nullptr) override;
  [[nodiscard]] error write_all(path_view location, std::span<const std::byte> bytes) override;

  [[nodiscard]] bool routes_paths() const noexcept override { return true; }
  [[nodiscard]] result<resolved_file> resolve_owned(path_view location) const override;
  [[nodiscard]] result<std::shared_ptr<file_system>>
  freeze_routes(detail::snapshot_context& context, std::shared_ptr<void> owner) const override;

private:
  struct route_entry {
    std::shared_ptr<void> owner;
    std::uint64_t source_id = 0;
    core::string scheme;
    core::string authority;
    core::string path_prefix;
    file_system* target = nullptr;
    core::string target_prefix;
  };

  [[nodiscard]] const route_entry* find_route(const core::vector<route_entry>& entries,
                                              uri_view location) const;
  [[nodiscard]] core::string resolve_path(const route_entry& entry, uri_view location) const;

  [[nodiscard]] error mount_owned(uri_view prefix, file_system& target, path_view target_prefix,
                                  std::shared_ptr<void> owner);
  using route_table = core::vector<route_entry>;
  memory_resource* resource_;
  ::caors::detail::shared_snapshot<const route_table> routes_;
};

} // namespace caors::io

#endif
