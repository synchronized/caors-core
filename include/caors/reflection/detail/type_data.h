// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_DETAIL_TYPE_DATA_H_
#define CAORS_REFLECTION_DETAIL_TYPE_DATA_H_

#include "caors/core/containers.h"
#include "caors/reflection/detail/field_wrapper.h"
#include "caors/reflection/detail/method_wrapper.h"

#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::reflection::detail {

using type_get_type_fn = meta::type (*)() noexcept;

/** @brief 类型反射函数表，保存由原始 C++ 类型生成的类型级操作。 */
struct type_ops {
  type_get_type_fn get_type = nullptr;

  [[nodiscard]] bool valid() const noexcept { return get_type != nullptr; }
};

/**
 * @brief 类型反射底层数据。
 *
 * 该类型保存反射组件自己的数据和由原始 C++ 类型生成的类型级函数表。基础类型记录仍由
 * `meta::registry` 统一索引，外部查询优先使用 `reflection::type` 只读视图。
 */
struct type_data {
  type_ops ops{};
  vector<field_wrapper> fields{};
  vector<method_wrapper> methods{};

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] meta::type get_type() const noexcept;
  [[nodiscard]] std::optional<field_wrapper> find_field(string_name name) const;
  [[nodiscard]] std::optional<field_wrapper> find_field(std::string_view name) const;
  [[nodiscard]] std::optional<method_wrapper> find_method(string_name name) const;
  [[nodiscard]] std::optional<method_wrapper> find_method(std::string_view name) const;
};

template <typename T> [[nodiscard]] meta::type reflected_type() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return meta::resolve<type>();
}

template <typename T> [[nodiscard]] type_ops make_type_ops() noexcept {
  return type_ops{&reflected_type<T>};
}

/** @brief 创建类型反射底层数据。 */
template <typename T>
[[nodiscard]] inline type_data make_type_data(std::initializer_list<field_wrapper> fields) {
  type_data data{};
  data.ops = make_type_ops<T>();
  data.fields = vector<field_wrapper>{fields};
  return data;
}

} // namespace caors::reflection::detail

#endif
