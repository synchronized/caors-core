// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_MODULE_RECORD_H_
#define CAORS_META_DETAIL_MODULE_RECORD_H_

#include "caors/meta/detail/method_record.h"
#include "caors/meta/detail/property_record.h"

namespace caors::meta::detail {

/** @brief 注册表内部保存的 module 数据，用于全局属性和全局函数。 */
class module_record final {
public:
  module_record() = default;
  explicit module_record(string_name module_name) noexcept : name_(module_name) {}

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] string_name name() const noexcept { return name_; }
  [[nodiscard]] std::size_t property_count() const noexcept;
  [[nodiscard]] const property_record* property_at(std::size_t index) const noexcept;
  [[nodiscard]] const property_record* find_property_ptr(string_name name) const noexcept;
  [[nodiscard]] error add_property(property_record property);
  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] const method_set_record* method_at(std::size_t index) const noexcept;
  [[nodiscard]] method_set_record* mutable_find_method_ptr(string_name name) noexcept;
  [[nodiscard]] const method_set_record* find_method_ptr(string_name name) const noexcept;
  [[nodiscard]] std::optional<std::size_t> add_method(method_record method);

private:
  string_name name_{};
  vector<property_record> properties_{};
  vector<method_set_record> methods_{};
};

} // namespace caors::meta::detail

#endif
