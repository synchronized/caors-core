// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DATA_H_
#define CAORS_META_DETAIL_TYPE_DATA_H_

#include "caors/core/containers.h"
#include "caors/error.h"
#include "caors/meta/detail/field_data.h"
#include "caors/meta/detail/method_data.h"
#include "caors/meta/type_registration.h"

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

class registry_state;

/** @brief 类型反射函数表，保存由原始 C++ 类型生成的类型级操作。 */
struct reflection_type_ops {
  using get_type_fn = meta::type (*)() noexcept;

  get_type_fn get_type = nullptr;

  [[nodiscard]] bool valid() const noexcept { return get_type != nullptr; }
};

/** @brief 注册表内部保存的完整 meta 类型数据。 */
class type_data {
public:
  type_data() = default;
  type_data(meta::type_registration_data registration) noexcept : registration_(registration) {}
  explicit type_data(reflection_type_ops ops) noexcept : reflection_ops_(ops) {}

  [[nodiscard]] type_id id() const noexcept { return registration_.id(); }
  [[nodiscard]] string_name name() const noexcept { return registration_.name(); }
  [[nodiscard]] type_guid guid() const noexcept { return registration_.guid(); }
  [[nodiscard]] type_id parent_id() const noexcept { return registration_.parent_id(); }

  [[nodiscard]] bool valid() const noexcept { return registration_.valid(); }
  [[nodiscard]] bool registered() const noexcept { return registration_.registered(); }

  void set_id(type_id id) noexcept { registration_.set_id(id); }
  void set_name(string_name name) noexcept { registration_.set_name(name); }
  void set_guid(type_guid guid) noexcept { registration_.set_guid(guid); }
  void set_parent_id(type_id parent_id) noexcept { registration_.set_parent_id(parent_id); }

  [[nodiscard]] bool has_reflection() const noexcept { return reflection_ops_.valid(); }
  [[nodiscard]] bool reflection_valid() const noexcept;
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
  [[nodiscard]] error attach_reflection(const type_data& reflection);
  [[nodiscard]] type_data* ensure_reflection(reflection_type_ops ops) noexcept;

private:
  meta::type_registration_data registration_{};
  reflection_type_ops reflection_ops_{};
  vector<field_data> fields_{};
  vector<method_data> methods_{};
};

[[nodiscard]] error merge_type_data(registry_state& state, type_data& stored,
                                    const type_data& incoming);

template <typename T> [[nodiscard]] meta::type reflected_type() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return meta::resolve<type>();
}

template <typename T> [[nodiscard]] reflection_type_ops make_reflection_type_ops() noexcept {
  return reflection_type_ops{&reflected_type<T>};
}

/** @brief 创建只携带反射字段/方法的类型数据，供反射兼容构建器提交使用。 */
template <typename T>
[[nodiscard]] inline type_data make_reflection_type_data(std::initializer_list<field_data> fields) {
  type_data data{make_reflection_type_ops<T>()};
  for (field_data field : fields) {
    data.add_field(field);
  }
  return data;
}

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(string_name name,
                                       type_id parent_id = type_id::invalid()) noexcept {
  return meta::make_type_registration_data<T>(name, parent_id);
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(string_name name, type_guid guid,
                                       type_id parent_id = type_id::invalid()) noexcept {
  return meta::make_type_registration_data<T>(name, guid, parent_id);
}

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(std::string_view name,
                                       type_id parent_id = type_id::invalid()) {
  return make_type_data<T>(string_name{name}, parent_id);
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(std::string_view name, type_guid guid,
                                       type_id parent_id = type_id::invalid()) {
  return make_type_data<T>(string_name{name}, guid, parent_id);
}

} // namespace caors::meta::detail

#endif
