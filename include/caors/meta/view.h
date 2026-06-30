// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VIEW_H_
#define CAORS_META_VIEW_H_

#include "caors/core/containers.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <optional>
#include <utility>

namespace caors {

class meta_registry;

/**
 * @brief 元数据组件查询视图。
 *
 * 视图保存查询时匹配的 `type_id` 列表，不持有组件引用。需要组件内容时可通过 `find<T>()`
 * 获取副本。
 */
template <typename... Components> class meta_view final {
public:
  using const_iterator = typename vector<type_id>::const_iterator;

  meta_view() = default;

  [[nodiscard]] const_iterator begin() const noexcept { return ids_.begin(); }
  [[nodiscard]] const_iterator end() const noexcept { return ids_.end(); }
  [[nodiscard]] bool empty() const noexcept { return ids_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return ids_.size(); }
  [[nodiscard]] const vector<type_id>& ids() const noexcept { return ids_; }

  /** @brief 获取视图中某个类型上的组件副本。 */
  template <typename Component> [[nodiscard]] std::optional<Component> find(type_id id) const;

private:
  friend class meta_registry;

  meta_view(const meta_registry* registry, vector<type_id> ids)
      : registry_(registry), ids_(std::move(ids)) {}

  const meta_registry* registry_ = nullptr;
  vector<type_id> ids_;
};

} // namespace caors

#endif
