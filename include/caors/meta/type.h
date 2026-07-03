// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_H_
#define CAORS_META_TYPE_H_

#include "caors/export.h"
#include "caors/meta/container_traits.h"
#include "caors/meta/type_key.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors {

class object;
class meta_registry;

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

/** @brief wrapper 类型的序列化语义。 */
enum class wrapper_semantics {
  none,
  inline_value,
  object_ref,
  unsupported,
};

namespace detail {

using value_default_construct_fn = void* (*)();
using value_clone_fn = void* (*)(const void*);
using value_destroy_fn = void (*)(void*) noexcept;
using value_assign_fn = bool (*)(void*, const void*);
using indirect_const_value_fn = const void* (*)(const void*) noexcept;
using indirect_mutable_value_fn = void* (*)(void*) noexcept;
using indirect_has_value_fn = bool (*)(const void*) noexcept;
using indirect_reset_fn = bool (*)(void*) noexcept;
using indirect_ensure_value_fn = bool (*)(void*);
using enum_to_i64_fn = std::int64_t (*)(const void*) noexcept;
using enum_to_u64_fn = std::uint64_t (*)(const void*) noexcept;
using enum_from_i64_fn = bool (*)(void*, std::int64_t) noexcept;
using enum_from_u64_fn = bool (*)(void*, std::uint64_t) noexcept;
using meta_object_ref_get_fn = object* (*)(const void*) noexcept;
using meta_object_ref_set_fn = bool (*)(void*, object*) noexcept;

template <typename T> struct meta_copy_constructible {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  static constexpr bool value = [] {
    if constexpr (adapters::sequence_traits<type>::is_sequence) {
      return meta_copy_constructible<typename adapters::sequence_traits<type>::element_type>::value;
    } else if constexpr (adapters::set_traits<type>::is_set) {
      return meta_copy_constructible<typename adapters::set_traits<type>::element_type>::value;
    } else if constexpr (adapters::associative_traits<type>::is_associative) {
      return meta_copy_constructible<
                 typename adapters::associative_traits<type>::key_type>::value &&
             meta_copy_constructible<
                 typename adapters::associative_traits<type>::mapped_type>::value;
    } else if constexpr (adapters::pair_traits<type>::is_pair) {
      return meta_copy_constructible<typename adapters::pair_traits<type>::first_type>::value &&
             meta_copy_constructible<typename adapters::pair_traits<type>::second_type>::value;
    } else if constexpr (adapters::variant_traits<type>::is_variant) {
      return []<std::size_t... Indices>(std::index_sequence<Indices...>) {
        return (meta_copy_constructible<typename adapters::variant_traits<
                    type>::template alternative_type<Indices>>::value &&
                ... && true);
      }(std::make_index_sequence<adapters::variant_traits<type>::size>{});
    } else {
      return std::is_copy_constructible_v<type>;
    }
  }();
};

template <typename T> struct meta_copy_assignable {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  static constexpr bool value = [] {
    if constexpr (adapters::sequence_traits<type>::is_sequence ||
                  adapters::set_traits<type>::is_set ||
                  adapters::associative_traits<type>::is_associative ||
                  adapters::pair_traits<type>::is_pair ||
                  adapters::variant_traits<type>::is_variant) {
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

} // namespace detail

/**
 * @brief 类型语义元数据。
 *
 * 该组件挂在字段声明类型对应的 `type_id` 上，集中描述 wrapper、指针和容器等运行期语义。反射、
 * 序列化和 value 模块通过同一份数据查询 value 类型和可选 key 类型，避免字段级重复存储。
 */
struct meta_type_semantics_data {
  type_kind kind = type_kind::value;
  wrapper_semantics wrapper_semantics = wrapper_semantics::none;
  type_id value_type{};
  type_id key_type{};
  detail::indirect_const_value_fn indirect_value = nullptr;
  detail::indirect_mutable_value_fn mutable_indirect_value = nullptr;
  detail::indirect_has_value_fn has_indirect_value = nullptr;
  detail::indirect_reset_fn reset_indirect_value = nullptr;
  detail::indirect_ensure_value_fn ensure_indirect_value = nullptr;
  detail::enum_to_i64_fn enum_to_i64 = nullptr;
  detail::enum_to_u64_fn enum_to_u64 = nullptr;
  detail::enum_from_i64_fn enum_from_i64 = nullptr;
  detail::enum_from_u64_fn enum_from_u64 = nullptr;
  detail::meta_object_ref_get_fn get_object_ref = nullptr;
  detail::meta_object_ref_set_fn set_object_ref = nullptr;
  std::uint8_t pointer_depth = 0;
  bool nullable = false;
  bool enum_signed = false;
};

/**
 * @brief 元类型底层数据。
 *
 * 该类型用于注册表内部存储和低层注册描述。对外查询应使用 `meta_type` 视图，避免直接依赖
 * 注册表内部数据布局。
 */
class meta_type_data {
public:
  constexpr meta_type_data() noexcept = default;

  constexpr meta_type_data(type_id id, string_name name, type_guid guid, std::size_t size,
                           std::size_t alignment, type_id parent_id,
                           detail::cpp_type_key cpp_type = nullptr,
                           detail::value_default_construct_fn default_construct_value = nullptr,
                           detail::value_clone_fn clone_value = nullptr,
                           detail::value_destroy_fn destroy_value = nullptr,
                           detail::value_assign_fn assign_value = nullptr) noexcept
      : id_(id), name_(name), guid_(guid), size_(size), alignment_(alignment),
        parent_id_(parent_id), cpp_type_(cpp_type),
        default_construct_value_(default_construct_value), clone_value_(clone_value),
        destroy_value_(destroy_value), assign_value_(assign_value) {}

  [[nodiscard]] constexpr type_id id() const noexcept { return id_; }
  [[nodiscard]] constexpr string_name name() const noexcept { return name_; }
  [[nodiscard]] constexpr type_guid guid() const noexcept { return guid_; }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
  [[nodiscard]] constexpr std::size_t alignment() const noexcept { return alignment_; }
  [[nodiscard]] constexpr type_id parent_id() const noexcept { return parent_id_; }
  [[nodiscard]] constexpr detail::cpp_type_key cpp_type() const noexcept { return cpp_type_; }
  [[nodiscard]] constexpr detail::value_default_construct_fn
  default_construct_value() const noexcept {
    return default_construct_value_;
  }
  [[nodiscard]] constexpr detail::value_clone_fn clone_value() const noexcept {
    return clone_value_;
  }
  [[nodiscard]] constexpr detail::value_destroy_fn destroy_value() const noexcept {
    return destroy_value_;
  }
  [[nodiscard]] constexpr detail::value_assign_fn assign_value() const noexcept {
    return assign_value_;
  }

  [[nodiscard]] constexpr bool valid() const noexcept { return size_ > 0 && alignment_ > 0; }

  [[nodiscard]] constexpr bool registered() const noexcept {
    return static_cast<bool>(id_) && valid();
  }

  constexpr void set_id(type_id id) noexcept { id_ = id; }
  constexpr void set_name(string_name name) noexcept { name_ = name; }
  constexpr void set_guid(type_guid guid) noexcept { guid_ = guid; }
  constexpr void set_parent_id(type_id parent_id) noexcept { parent_id_ = parent_id; }
  constexpr void set_cpp_type(detail::cpp_type_key cpp_type) noexcept { cpp_type_ = cpp_type; }
  constexpr void
  set_default_construct_value(detail::value_default_construct_fn default_construct_value) noexcept {
    default_construct_value_ = default_construct_value;
  }
  constexpr void set_clone_value(detail::value_clone_fn clone_value) noexcept {
    clone_value_ = clone_value;
  }
  constexpr void set_destroy_value(detail::value_destroy_fn destroy_value) noexcept {
    destroy_value_ = destroy_value;
  }
  constexpr void set_assign_value(detail::value_assign_fn assign_value) noexcept {
    assign_value_ = assign_value;
  }

private:
  type_id id_{};
  string_name name_{};
  type_guid guid_{};
  std::size_t size_ = 0;
  std::size_t alignment_ = 0;
  type_id parent_id_{};
  detail::cpp_type_key cpp_type_ = nullptr;
  detail::value_default_construct_fn default_construct_value_ = nullptr;
  detail::value_clone_fn clone_value_ = nullptr;
  detail::value_destroy_fn destroy_value_ = nullptr;
  detail::value_assign_fn assign_value_ = nullptr;
};

/**
 * @brief 元类型只读视图。
 *
 * `meta_type` 不拥有类型数据，只引用全局 `meta_registry` 中 append-only 保存的
 * `meta_type_data`。视图复制成本低，适合作为对外查询结果。
 */
class CAORS_API meta_type {
public:
  constexpr meta_type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;
  [[nodiscard]] type_id parent_id() const noexcept;
  [[nodiscard]] detail::cpp_type_key cpp_type() const noexcept;
  [[nodiscard]] detail::value_default_construct_fn default_construct_value() const noexcept;
  [[nodiscard]] detail::value_clone_fn clone_value() const noexcept;
  [[nodiscard]] detail::value_destroy_fn destroy_value() const noexcept;
  [[nodiscard]] detail::value_assign_fn assign_value() const noexcept;
  [[nodiscard]] meta_type parent() const noexcept;

private:
  friend class meta_registry;

  constexpr meta_type(const meta_registry* registry, const meta_type_data* data) noexcept
      : registry_(registry), data_(data) {}

  const meta_registry* registry_ = nullptr;
  const meta_type_data* data_ = nullptr;
};

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] constexpr meta_type_data
make_meta_type_data(string_name name, type_id parent_id = type_id::invalid()) noexcept {
  return meta_type_data{
      type_id::invalid(),
      name,
      type_guid::nil(),
      sizeof(T),
      alignof(T),
      parent_id,
      detail::cpp_type_key_of<T>(),
      detail::default_construct_fn_of<T>(),
      detail::value_clone_fn_of<T>(),
      detail::value_destroy_fn_of<T>(),
      detail::value_assign_fn_of<T>(),
  };
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] constexpr meta_type_data
make_meta_type_data(string_name name, type_guid guid,
                    type_id parent_id = type_id::invalid()) noexcept {
  return meta_type_data{
      type_id::invalid(),
      name,
      guid,
      sizeof(T),
      alignof(T),
      parent_id,
      detail::cpp_type_key_of<T>(),
      detail::default_construct_fn_of<T>(),
      detail::value_clone_fn_of<T>(),
      detail::value_destroy_fn_of<T>(),
      detail::value_assign_fn_of<T>(),
  };
}

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] meta_type_data make_meta_type_data(std::string_view name,
                                                 type_id parent_id = type_id::invalid()) {
  return make_meta_type_data<T>(string_name{name}, parent_id);
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] meta_type_data make_meta_type_data(std::string_view name, type_guid guid,
                                                 type_id parent_id = type_id::invalid()) {
  return make_meta_type_data<T>(string_name{name}, guid, parent_id);
}

} // namespace caors

#endif
