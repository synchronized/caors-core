// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_CALL_GUARD_H_
#define CAORS_META_DETAIL_CALL_GUARD_H_

#include "caors/export.h"

#include <memory>

namespace caors::meta {

class type;

namespace detail {

class registry_layer_state;

/** @brief 保证一次类型擦除调用执行期间所属 layer 保持存活。 */
class layer_call_guard final {
public:
  explicit CAORS_API layer_call_guard(const meta::type& type) noexcept;
  explicit CAORS_API layer_call_guard(std::shared_ptr<registry_layer_state> layer) noexcept;
  CAORS_API ~layer_call_guard();

  layer_call_guard(const layer_call_guard&) = delete;
  layer_call_guard& operator=(const layer_call_guard&) = delete;

  [[nodiscard]] CAORS_API bool valid() const noexcept;
  [[nodiscard]] explicit CAORS_API operator bool() const noexcept;

private:
  std::shared_ptr<registry_layer_state> layer_{};
  bool entered_ = false;
};

} // namespace detail
} // namespace caors::meta

#endif
