// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_TYPE_H_
#define CAORS_REFLECTION_TYPE_H_

#include "caors/reflection/field.h"
#include "caors/reflection/method.h"

#include <cstddef>
#include <string_view>

namespace caors::reflection {

class registry;

namespace detail {

struct type_data;

} // namespace detail

/**
 * @brief 类型反射只读视图。
 *
 * `reflection::type` 不拥有反射数据，只引用全局注册表中 append-only 保存的 `meta::type` 和
 * 内部反射数据。有效视图可以在进程生命周期内稳定使用。
 */
class CAORS_API type final {
public:
  type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta::type info() const noexcept;
  [[nodiscard]] type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] std::size_t field_count() const noexcept;
  [[nodiscard]] field field_at(std::size_t index) const;
  [[nodiscard]] field find_field(string_name name) const;
  [[nodiscard]] field find_field(std::string_view name) const;
  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] method method_at(std::size_t index) const;
  [[nodiscard]] method find_method(string_name name) const;
  [[nodiscard]] method find_method(std::string_view name) const;

private:
  friend class registry;

  type(meta::type info, const detail::type_data* data) noexcept;
  [[nodiscard]] const detail::type_data* data() const noexcept;

  meta::type type_{};
  const detail::type_data* data_ = nullptr;
};

} // namespace caors::reflection

#endif
