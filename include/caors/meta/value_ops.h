// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_OPS_H_
#define CAORS_META_VALUE_OPS_H_

#include "caors/export.h"
#include "caors/memory/resource.h"
#include "caors/traits/container.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::meta {

class type;

using value_default_construct_fn = void* (*)();
using value_clone_fn = void* (*)(const void*);
using value_destroy_fn = void (*)(void*) noexcept;
using value_assign_fn = bool (*)(void*, const void*);
using value_resource_construct_fn = void* (*)(memory::memory_resource*);
using value_resource_destroy_fn = void (*)(memory::memory_resource*, void*) noexcept;

/** @brief C++ 类型布局信息。 */
struct type_layout {
  std::size_t size = 0;
  std::size_t alignment = 0;

  [[nodiscard]] bool valid() const noexcept { return size > 0 && alignment > 0; }
};

/** @brief C++ 值的类型擦除操作函数表。 */
struct value_ops {
  value_default_construct_fn default_construct = nullptr;
  value_clone_fn clone = nullptr;
  value_destroy_fn destroy = nullptr;
  value_assign_fn assign = nullptr;
  value_resource_construct_fn resource_construct = nullptr;
  value_resource_destroy_fn resource_destroy = nullptr;
};

/** @brief C++ 值的编译期特征摘要。 */
struct value_traits {
  bool trivially_copyable = false;
  bool trivially_destructible = false;
  bool trivially_default_constructible = false;
};

/** @brief C++ 值的布局、生命周期和存储相关操作。 */
struct value_storage_ops {
  type_layout layout{};
  value_ops ops{};
  value_traits traits{};

  [[nodiscard]] bool valid() const noexcept { return layout.valid(); }
};

/** @brief C++ 值的可选资源感知构造策略。 */
struct value_factory_ops {
  value_resource_construct_fn construct = nullptr;
  value_resource_destroy_fn destroy = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return construct != nullptr && destroy != nullptr;
  }
};

[[nodiscard]] CAORS_API value_default_construct_fn
get_default_construct_value_fn(const meta::type& type) noexcept;
[[nodiscard]] CAORS_API value_clone_fn get_clone_value_fn(const meta::type& type) noexcept;
[[nodiscard]] CAORS_API value_destroy_fn get_destroy_value_fn(const meta::type& type) noexcept;
[[nodiscard]] CAORS_API value_assign_fn get_assign_value_fn(const meta::type& type) noexcept;
[[nodiscard]] CAORS_API value_resource_construct_fn
get_resource_construct_value_fn(const meta::type& type) noexcept;
[[nodiscard]] CAORS_API value_resource_destroy_fn
get_resource_destroy_value_fn(const meta::type& type) noexcept;
[[nodiscard]] CAORS_API const value_storage_ops*
get_value_storage_ops(const meta::type& type) noexcept;

namespace detail {

using meta::get_assign_value_fn;
using meta::get_clone_value_fn;
using meta::get_default_construct_value_fn;
using meta::get_destroy_value_fn;
using meta::get_resource_construct_value_fn;
using meta::get_resource_destroy_value_fn;
using meta::get_value_storage_ops;
using meta::value_assign_fn;
using meta::value_clone_fn;
using meta::value_default_construct_fn;
using meta::value_destroy_fn;
using meta::value_factory_ops;
using meta::value_resource_construct_fn;
using meta::value_resource_destroy_fn;
using meta::value_storage_ops;

template <typename T> struct meta_copy_constructible;

template <typename T, bool = traits::sequence_traits<T>::is_sequence>
struct sequence_meta_copy_constructible {
  static constexpr bool value = false;
};

template <typename T> struct sequence_meta_copy_constructible<T, true> {
  static constexpr bool value =
      meta_copy_constructible<typename traits::sequence_traits<T>::element_type>::value;
};

template <typename T, bool = traits::set_traits<T>::is_set> struct set_meta_copy_constructible {
  static constexpr bool value = false;
};

template <typename T> struct set_meta_copy_constructible<T, true> {
  static constexpr bool value =
      meta_copy_constructible<typename traits::set_traits<T>::element_type>::value;
};

template <typename T, bool = traits::associative_traits<T>::is_associative>
struct associative_meta_copy_constructible {
  static constexpr bool value = false;
};

template <typename T> struct associative_meta_copy_constructible<T, true> {
  static constexpr bool value =
      meta_copy_constructible<typename traits::associative_traits<T>::key_type>::value &&
      meta_copy_constructible<typename traits::associative_traits<T>::mapped_type>::value;
};

template <typename T, bool = traits::pair_traits<T>::is_pair> struct pair_meta_copy_constructible {
  static constexpr bool value = false;
};

template <typename T> struct pair_meta_copy_constructible<T, true> {
  static constexpr bool value =
      meta_copy_constructible<typename traits::pair_traits<T>::first_type>::value &&
      meta_copy_constructible<typename traits::pair_traits<T>::second_type>::value;
};

template <typename T, bool = traits::variant_traits<T>::is_variant>
struct variant_meta_copy_constructible {
  static constexpr bool value = false;
};

template <typename T> struct variant_meta_copy_constructible<T, true> {
  static constexpr bool value = []<std::size_t... Indices>(std::index_sequence<Indices...>) {
    return (meta_copy_constructible<
                typename traits::variant_traits<T>::template alternative_type<Indices>>::value &&
            ... && true);
  }(std::make_index_sequence<traits::variant_traits<T>::size>{});
};

template <typename T> [[nodiscard]] consteval bool meta_copy_constructible_value() {
  if constexpr (traits::sequence_traits<T>::is_sequence) {
    return sequence_meta_copy_constructible<T>::value;
  } else if constexpr (traits::set_traits<T>::is_set) {
    return set_meta_copy_constructible<T>::value;
  } else if constexpr (traits::associative_traits<T>::is_associative) {
    return associative_meta_copy_constructible<T>::value;
  } else if constexpr (traits::pair_traits<T>::is_pair) {
    return pair_meta_copy_constructible<T>::value;
  } else if constexpr (traits::variant_traits<T>::is_variant) {
    return variant_meta_copy_constructible<T>::value;
  } else {
    return std::is_copy_constructible_v<T>;
  }
}

template <typename T> struct meta_copy_constructible {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  static constexpr bool value = meta_copy_constructible_value<type>();
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

template <typename T> void* resource_default_construct_value(memory::memory_resource* resource) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_default_constructible_v<type>) {
    return memory::make<type>(resource);
  } else {
    return nullptr;
  }
}

template <typename T>
void resource_destroy_value(memory::memory_resource* resource, void* value) noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  memory::destroy(static_cast<type*>(value), resource);
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

template <typename T>
[[nodiscard]] value_resource_construct_fn resource_default_construct_fn_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_default_constructible_v<type>) {
    return &resource_default_construct_value<type>;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] value_resource_destroy_fn resource_destroy_fn_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_destructible_v<type>) {
    return &resource_destroy_value<type>;
  } else {
    return nullptr;
  }
}

} // namespace detail

/** @brief 自定义类型工厂构造函数；可选择使用传入的内存资源。 */
using type_factory_construct_fn = value_resource_construct_fn;

/** @brief 自定义类型工厂销毁函数；必须匹配对应构造函数的分配策略。 */
using type_factory_destroy_fn = value_resource_destroy_fn;

} // namespace caors::meta

#endif
