// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_REFLECTION_DATA_H_
#define CAORS_META_DETAIL_REFLECTION_DATA_H_

#include "caors/core/containers.h"
#include "caors/meta/detail/field_data.h"
#include "caors/meta/detail/method_data.h"

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::meta::detail {

/** @brief 类型反射函数表，保存由原始 C++ 类型生成的类型级操作。 */
struct reflection_type_ops {
  using get_type_fn = meta::type (*)() noexcept;

  get_type_fn get_type = nullptr;

  [[nodiscard]] bool valid() const noexcept { return get_type != nullptr; }
};

/**
 * @brief 字段和方法内部数据。
 *
 * 该类型保存由原始 C++ 类型生成的类型级函数表，以及注册到 meta 类型上的字段和方法列表。
 * 外部查询优先使用 `meta::type` / `meta::field` / `meta::method` 只读视图。
 */
class reflection_data {
public:
  reflection_data() = default;
  explicit reflection_data(reflection_type_ops ops) noexcept;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] meta::type get_type() const noexcept;
  [[nodiscard]] std::size_t field_count() const noexcept;
  [[nodiscard]] const field_data* field_at(std::size_t index) const noexcept;
  [[nodiscard]] field_data* mutable_field_at(std::size_t index) noexcept;
  [[nodiscard]] const field_data* find_field_ptr(string_name name) const noexcept;
  [[nodiscard]] field_data* mutable_find_field_ptr(string_name name) noexcept;
  [[nodiscard]] std::optional<field_data> find_field(string_name name) const;
  [[nodiscard]] std::optional<field_data> find_field(std::string_view name) const;
  void add_field(field_data field);
  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] const method_data* method_at(std::size_t index) const noexcept;
  [[nodiscard]] const method_data* find_method_ptr(string_name name) const noexcept;
  [[nodiscard]] std::optional<method_data> find_method(string_name name) const;
  [[nodiscard]] std::optional<method_data> find_method(std::string_view name) const;
  void add_method(method_data method);

private:
  reflection_type_ops ops_{};
  vector<field_data> fields_{};
  vector<method_data> methods_{};
};

template <typename T> [[nodiscard]] meta::type reflected_type() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return meta::resolve<type>();
}

template <typename T> [[nodiscard]] reflection_type_ops make_reflection_type_ops() noexcept {
  return reflection_type_ops{&reflected_type<T>};
}

/** @brief 创建类型反射底层数据。 */
template <typename T>
[[nodiscard]] inline reflection_data make_reflection_data(std::initializer_list<field_data> fields) {
  reflection_data wrapper{make_reflection_type_ops<T>()};
  for (field_data field : fields) {
    wrapper.add_field(field);
  }
  return wrapper;
}

} // namespace caors::meta::detail

#endif
