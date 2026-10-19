// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_ROUTE_SNAPSHOT_H_
#define CAORS_IO_ROUTE_SNAPSHOT_H_

#include "caors/io/types.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace caors::io {
class file_system;

/** @brief 拥有后端寿命和路径的解析结果；allocator 必须覆盖本对象及后端的真实清理。 */
struct resolved_file {
  /** @brief 持有后端和挂载提供的模块/宿主凭据。 */
  std::shared_ptr<file_system> backend;
  /** @brief 后端自身命名空间中的路径，不借用输入字符串。 */
  caors::io::path path{};
  /** @brief 进程内挂载实例标记；不能用作持久标识或内容摘要。 */
  std::uint64_t source_id = 0;
};

/** @brief 只读路由快照；可跨线程解析，具体后端的线程约束独立成立。 */
class CAORS_API route_snapshot {
public:
  route_snapshot() = default;
  /** @brief 捕获拥有式根及嵌套路由；借用边或不支持快照的路由后端返回 unsupported。 */
  [[nodiscard]] static result<route_snapshot> capture(std::shared_ptr<file_system> root,
                                                      std::shared_ptr<void> lifetime = {});
  /** @brief 解析拥有式路径；空快照返回 invalid_argument，不读取文件内容。 */
  [[nodiscard]] result<resolved_file> resolve(path_view location) const;
  /** @brief 快照是否有效。 */
  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(root_); }

private:
  friend class file_system;
  explicit route_snapshot(std::shared_ptr<file_system> root) : root_(std::move(root)) {}
  std::shared_ptr<file_system> root_;
};

namespace detail {
/** @brief 快照扩展上下文；自定义路由实现须通过 capture 递归，以统一检测循环和共享节点。 */
class CAORS_API snapshot_context {
public:
  /** @brief 捕获一个节点，owner 必须覆盖后端及必要宿主凭据；仅内置路由根允许无 owner。 */
  [[nodiscard]] result<std::shared_ptr<file_system>> capture(const file_system& target,
                                                             std::shared_ptr<void> owner);

private:
  std::vector<const file_system*> active_;
  std::unordered_map<const file_system*, std::shared_ptr<file_system>> captured_;
};
[[nodiscard]] CAORS_API std::uint64_t next_route_id() noexcept;
[[nodiscard]] CAORS_API result<resolved_file>
resolve_snapshot(const std::shared_ptr<file_system>& root, path_view location);
} // namespace detail
} // namespace caors::io
#endif
