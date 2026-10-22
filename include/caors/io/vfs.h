// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_VFS_H_
#define CAORS_IO_VFS_H_
#include "caors/io/backend.h"
#include "caors/io/route_snapshot.h"
#include "caors/io/uri.h"
namespace caors::io {
class file_system_resolver;
class io_context;
/** @brief 拥有式不可变路由快照；固定后端和路由，不冻结文件内容或 allocator。 */
class CAORS_API vfs_snapshot {
public:
  vfs_snapshot() = default;

private:
  friend class vfs;
  friend class io_context;
  [[nodiscard]] result<resolved_file> resolve(path_view location) const;
  explicit vfs_snapshot(route_snapshot value) : routes_(std::move(value)) {}
  route_snapshot routes_;
};
/** @brief 挂载配置入口，不继承文件后端；拥有式挂载更新可与快照捕获并发。 */
class CAORS_API vfs {
public:
  explicit vfs(memory_resource* resource = nullptr);
  /** @brief 挂载 URI 或普通虚拟前缀；重复前缀失败，不替换旧挂载。 */
  [[nodiscard]] error mount(uri_view prefix, std::shared_ptr<backend> target,
                            path_view target_prefix = {}, std::shared_ptr<void> lifetime = {});
  /** @brief 取消挂载，不影响已有请求和快照。 */
  [[nodiscard]] bool unmount(uri_view prefix);
  /** @brief 清空活跃挂载，已捕获的批次仍持有原后端。 */
  void clear();
  /** @brief 查询当前挂载数。 */
  [[nodiscard]] std::size_t mount_count() const noexcept;
  /** @brief 捕获本批路由与后端寿命；挂载之外的后端状态由宿主管理。 */
  [[nodiscard]] result<vfs_snapshot> snapshot() const;

private:
  std::shared_ptr<file_system_resolver> routes_;
};
} // namespace caors::io
#endif
