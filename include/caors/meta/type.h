// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_H_
#define CAORS_META_TYPE_H_

#include "caors/core/string_name.h"
#include "caors/error.h"
#include "caors/export.h"
#include "caors/memory/resource.h"
#include "caors/meta/capability.h"
#include "caors/meta/detail/type_data.h"
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
class type;
struct type_descriptor;

[[nodiscard]] CAORS_API registry& get_registry() noexcept;

namespace detail {

/** @brief 从类型 id 和 descriptor 构造类型视图。内部用于字段和运行期值视图。 */
[[nodiscard]] CAORS_API type make_type_view(type_id id,
                                            const meta::type_descriptor* descriptor) noexcept;

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
using value_resource_construct_fn = void* (*)(memory::memory_resource*);
using value_resource_destroy_fn = void (*)(memory::memory_resource*, void*) noexcept;

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
using type_factory_construct_fn = detail::value_resource_construct_fn;

/** @brief 自定义类型工厂销毁函数；必须匹配对应构造函数的分配策略。 */
using type_factory_destroy_fn = detail::value_resource_destroy_fn;

/**
 * @brief 元类型只读视图。
 *
 * `meta::type` 不拥有类型数据，只引用编译期生成的 type descriptor，并在类型已注册时额外引用
 * 全局 `meta::registry` 中 append-only 保存的注册数据。视图复制成本低，适合作为对外查询结果。
 */
class CAORS_API type {
public:
  constexpr type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool registered() const noexcept;

  [[nodiscard]] type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;
  [[nodiscard]] type_id parent_id() const noexcept;
  [[nodiscard]] type parent() const noexcept;

  /** @brief 查询该类型视图具备的能力；优先使用 descriptor 静态能力，再回退到运行期索引。 */
  template <typename Capability> [[nodiscard]] const Capability* get_capability() const noexcept {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    return static_cast<const capability_type*>(
        detail::find_type_capability(*this, capability_key_of<capability_type>()));
  }

  /** @brief 判断该类型视图是否具备指定能力。 */
  template <typename Capability> [[nodiscard]] bool has_capability() const noexcept {
    return get_capability<Capability>() != nullptr;
  }

private:
  friend class registry;
  friend detail::value_default_construct_fn
  detail::get_default_construct_value_fn(const type&) noexcept;
  friend detail::value_clone_fn detail::get_clone_value_fn(const type&) noexcept;
  friend detail::value_destroy_fn detail::get_destroy_value_fn(const type&) noexcept;
  friend detail::value_assign_fn detail::get_assign_value_fn(const type&) noexcept;
  friend type detail::make_type_view(type_id, const meta::type_descriptor*) noexcept;
  friend const void* detail::find_type_capability(const type&, capability_key) noexcept;

  constexpr type(const registry* registry, const detail::type_data* data,
                 const meta::type_descriptor* descriptor) noexcept
      : registry_(registry), data_(data), descriptor_(descriptor) {}

  const registry* registry_ = nullptr;
  const detail::type_data* data_ = nullptr;
  const meta::type_descriptor* descriptor_ = nullptr;
};

} // namespace caors::meta

#endif
