// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_H_
#define CAORS_META_TYPE_H_

#include "caors/core/string_name.h"
#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta/type_id.h"
#include "caors/traits/container.h"
#include "caors/traits/wrapper.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta {

class registry;
class registry_state;
class type;

namespace detail {

class type_data;
struct type_descriptor;

} // namespace detail

/** @brief 类型运行期形态。 */
enum class type_kind {
  value,
  enumeration,
  raw_pointer,
  wrapper,
  sequence,
  set,
  associative,
  pair,
  tuple,
  variant,
};

using wrapper_semantics = traits::wrapper_semantics;

namespace detail {

using value_default_construct_fn = void* (*)();
using value_clone_fn = void* (*)(const void*);
using value_destroy_fn = void (*)(void*) noexcept;
using value_assign_fn = bool (*)(void*, const void*);

[[nodiscard]] CAORS_API value_default_construct_fn
get_default_construct_value_fn(const meta::type& type) noexcept;
[[nodiscard]] CAORS_API value_clone_fn get_clone_value_fn(const meta::type& type) noexcept;
[[nodiscard]] CAORS_API value_destroy_fn get_destroy_value_fn(const meta::type& type) noexcept;
[[nodiscard]] CAORS_API value_assign_fn get_assign_value_fn(const meta::type& type) noexcept;

[[nodiscard]] error merge_type_data(registry_state& state, type_data& stored,
                                    const type_data& incoming);

template <typename T> struct meta_copy_constructible {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  static constexpr bool value = [] {
    if constexpr (traits::sequence_traits<type>::is_sequence) {
      return meta_copy_constructible<typename traits::sequence_traits<type>::element_type>::value;
    } else if constexpr (traits::set_traits<type>::is_set) {
      return meta_copy_constructible<typename traits::set_traits<type>::element_type>::value;
    } else if constexpr (traits::associative_traits<type>::is_associative) {
      return meta_copy_constructible<typename traits::associative_traits<type>::key_type>::value &&
             meta_copy_constructible<typename traits::associative_traits<type>::mapped_type>::value;
    } else if constexpr (traits::pair_traits<type>::is_pair) {
      return meta_copy_constructible<typename traits::pair_traits<type>::first_type>::value &&
             meta_copy_constructible<typename traits::pair_traits<type>::second_type>::value;
    } else if constexpr (traits::variant_traits<type>::is_variant) {
      return []<std::size_t... Indices>(std::index_sequence<Indices...>) {
        return (
            meta_copy_constructible<
                typename traits::variant_traits<type>::template alternative_type<Indices>>::value &&
            ... && true);
      }(std::make_index_sequence<traits::variant_traits<type>::size>{});
    } else {
      return std::is_copy_constructible_v<type>;
    }
  }();
};

template <typename T> struct meta_copy_assignable {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  static constexpr bool value = [] {
    if constexpr (traits::sequence_traits<type>::is_sequence || traits::set_traits<type>::is_set ||
                  traits::associative_traits<type>::is_associative ||
                  traits::pair_traits<type>::is_pair || traits::variant_traits<type>::is_variant) {
      if constexpr (!meta_copy_constructible<type>::value) {
        return false;
      } else {
        return std::is_assignable_v<type&, const type&>;
      }
    } else {
      return std::is_assignable_v<type&, const type&>;
    }
  }();
};

template <typename T> void* default_construct_value() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_default_constructible_v<type>) {
    return new type();
  } else {
    return nullptr;
  }
}

template <typename T> void* clone_value(const void* source) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (meta_copy_constructible<type>::value) {
    return new type(*static_cast<const type*>(source));
  } else {
    return nullptr;
  }
}

template <typename T> void destroy_value(void* value) noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  delete static_cast<type*>(value);
}

template <typename T> bool assign_value(void* target, const void* source) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (meta_copy_assignable<type>::value) {
    *static_cast<type*>(target) = *static_cast<const type*>(source);
    return true;
  } else {
    return false;
  }
}

template <typename T> [[nodiscard]] value_default_construct_fn default_construct_fn_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_default_constructible_v<type>) {
    return &default_construct_value<type>;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] value_clone_fn value_clone_fn_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (meta_copy_constructible<type>::value) {
    return &clone_value<type>;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] value_destroy_fn value_destroy_fn_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_destructible_v<type>) {
    return &destroy_value<type>;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] value_assign_fn value_assign_fn_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (meta_copy_assignable<type>::value) {
    return &assign_value<type>;
  } else {
    return nullptr;
  }
}

/**
 * @brief 元类型底层数据。
 *
 * 该类型用于注册表内部存储和低层注册描述。对外查询应使用 `meta::type` 视图，避免直接依赖
 * 注册表内部数据布局。
 */
class type_data {
public:
  constexpr type_data() noexcept = default;

  constexpr type_data(type_id id, string_name name, type_guid guid, type_id parent_id) noexcept
      : id_(id), name_(name), guid_(guid), parent_id_(parent_id) {}

  [[nodiscard]] constexpr type_id id() const noexcept { return id_; }
  [[nodiscard]] constexpr string_name name() const noexcept { return name_; }
  [[nodiscard]] constexpr type_guid guid() const noexcept { return guid_; }
  [[nodiscard]] constexpr type_id parent_id() const noexcept { return parent_id_; }

  [[nodiscard]] constexpr bool valid() const noexcept { return static_cast<bool>(id_); }

  [[nodiscard]] constexpr bool registered() const noexcept {
    return static_cast<bool>(id_) && valid();
  }

  constexpr void set_id(type_id id) noexcept { id_ = id; }
  constexpr void set_name(string_name name) noexcept { name_ = name; }
  constexpr void set_guid(type_guid guid) noexcept { guid_ = guid; }
  constexpr void set_parent_id(type_id parent_id) noexcept { parent_id_ = parent_id; }

private:
  friend error merge_type_data(registry_state& state, type_data& stored, const type_data& incoming);

  type_id id_{};
  string_name name_{};
  type_guid guid_{};
  type_id parent_id_{};
};

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(string_name name,
                                       type_id parent_id = type_id::invalid()) noexcept {
  return type_data{
      type_id_of_cpp_type<T>(),
      name,
      type_guid::nil(),
      parent_id,
  };
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(string_name name, type_guid guid,
                                       type_id parent_id = type_id::invalid()) noexcept {
  return type_data{
      type_id_of_cpp_type<T>(),
      name,
      guid,
      parent_id,
  };
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

} // namespace detail

/**
 * @brief 元类型只读视图。
 *
 * `meta::type` 不拥有类型数据，只引用全局 `meta::registry` 中 append-only 保存的
 * 内部类型数据。视图复制成本低，适合作为对外查询结果。
 */
class CAORS_API type {
public:
  constexpr type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;
  [[nodiscard]] type_id parent_id() const noexcept;
  [[nodiscard]] type parent() const noexcept;

private:
  friend class registry;
  friend detail::value_default_construct_fn
  detail::get_default_construct_value_fn(const type&) noexcept;
  friend detail::value_clone_fn detail::get_clone_value_fn(const type&) noexcept;
  friend detail::value_destroy_fn detail::get_destroy_value_fn(const type&) noexcept;
  friend detail::value_assign_fn detail::get_assign_value_fn(const type&) noexcept;

  constexpr type(const registry* registry, const detail::type_data* data) noexcept
      : registry_(registry), data_(data) {}

  const registry* registry_ = nullptr;
  const detail::type_data* data_ = nullptr;
};

} // namespace caors::meta

#endif
