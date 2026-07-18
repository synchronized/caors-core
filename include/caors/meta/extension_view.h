// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_EXTENSION_VIEW_H_
#define CAORS_META_EXTENSION_VIEW_H_

#include "caors/core/containers.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <optional>
#include <utility>

namespace caors::meta {

class registry;

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

  /** @brief 获取视图中某个类型上的扩展数据副本。 */
  template <typename Extension> [[nodiscard]] std::optional<Extension> find(type_id id) const;

private:
  friend class registry;

  extension_view(const registry* owner, vector<type_id> ids)
      : registry_(owner), ids_(std::move(ids)) {}

  const registry* registry_ = nullptr;
  vector<type_id> ids_;
};

} // namespace caors::meta

#endif
