// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_EXTENSION_VIEW_H_
#define CAORS_META_EXTENSION_VIEW_H_

#include "caors/core/containers.h"
#include "caors/export.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

namespace caors::meta {

class registry;
namespace detail {
class registry_snapshot;
[[nodiscard]] CAORS_API bool
registry_snapshot_active(const std::shared_ptr<registry_snapshot>& snapshot) noexcept;
}

/**
 * @brief 元类型扩展数据查询视图。
 *
 * 视图保存查询时匹配的 `type_id` 列表，不持有扩展数据引用。需要内容时可通过 `find<T>()`
 * 获取副本。
 */
template <typename... Extensions> class extension_view final {
public:
  using const_iterator = typename vector<type_id>::const_iterator;

  extension_view() = default;

  [[nodiscard]] const_iterator begin() const noexcept { return ids_.begin(); }
  [[nodiscard]] const_iterator end() const noexcept { return ids_.end(); }
  [[nodiscard]] bool empty() const noexcept { return ids_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return ids_.size(); }
  [[nodiscard]] const vector<type_id>& ids() const noexcept { return ids_; }
  [[nodiscard]] bool valid() const noexcept { return detail::registry_snapshot_active(snapshot_); }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  /** @brief 获取视图中某个类型上的扩展数据副本。 */
  template <typename Extension> [[nodiscard]] std::optional<Extension> find(type_id id) const;

private:
  friend class registry;

  extension_view(const registry* owner, vector<type_id> ids,
                 std::shared_ptr<detail::registry_snapshot> snapshot)
      : registry_(owner), ids_(std::move(ids)), snapshot_(std::move(snapshot)) {}

  const registry* registry_ = nullptr;
  vector<type_id> ids_;
  std::shared_ptr<detail::registry_snapshot> snapshot_{};
};

} // namespace caors::meta

#endif
