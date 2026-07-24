// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_MODULE_DATA_H_
#define CAORS_META_DETAIL_MODULE_DATA_H_

#include "caors/meta/detail/method_data.h"
#include "caors/meta/detail/property_data.h"

namespace caors::meta::detail {

/** @brief 注册表内部保存的 module 数据，用于全局属性和全局函数。 */
class module_data final {
public:
  module_data() = default;
  explicit module_data(string_name module_name) noexcept : name_(module_name) {}

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] string_name name() const noexcept { return name_; }
  [[nodiscard]] std::size_t property_count() const noexcept;
  [[nodiscard]] const property_data* property_at(std::size_t index) const noexcept;
  [[nodiscard]] const property_data* find_property_ptr(string_name name) const noexcept;
  [[nodiscard]] error add_property(property_data property);
  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] const method_set_data* method_at(std::size_t index) const noexcept;
  [[nodiscard]] method_set_data* mutable_find_method_ptr(string_name name) noexcept;
  [[nodiscard]] const method_set_data* find_method_ptr(string_name name) const noexcept;
  [[nodiscard]] std::optional<std::size_t> add_method(method_data method);

private:
  string_name name_{};
  vector<property_data> properties_{};
  vector<method_set_data> methods_{};
};

} // namespace caors::meta::detail

#endif
