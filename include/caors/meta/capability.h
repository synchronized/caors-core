// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CAPABILITY_H_
#define CAORS_META_CAPABILITY_H_

#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <type_traits>

namespace caors::meta {

/** @brief 运行期能力类型的唯一 key。 */
using capability_key = const void*;

/** @brief 一条静态能力记录。 */
struct capability_entry {
  capability_key key = nullptr;
  const void* data = nullptr;

  [[nodiscard]] bool valid() const noexcept { return key != nullptr && data != nullptr; }
};

/** @brief 返回能力类型在当前进程内的唯一 key。 */
template <typename Capability> [[nodiscard]] capability_key capability_key_of() noexcept {
  static const int key = 0;
  return &key;
}

namespace detail {

[[nodiscard]] CAORS_API error register_capability(type_id id, capability_key key,
                                                  const void* capability);
[[nodiscard]] CAORS_API const void* find_capability(type_id id, capability_key key) noexcept;
[[nodiscard]] CAORS_API bool contains_capability(type_id id, capability_key key) noexcept;
[[nodiscard]] CAORS_API std::size_t capability_count(capability_key key) noexcept;

} // namespace detail

namespace capabilities {

/**
 * @brief 编译期静态能力提供者。
 *
 * 模块可特化该模板，让 `get<Capability, T>()` 在拿得到 C++ 类型 `T`
 * 时直接返回静态能力，不依赖运行期注册表。默认表示没有该能力。
 */
template <typename Capability, typename T> struct static_capability {
  [[nodiscard]] static const Capability* get() { return nullptr; }
};

/**
 * @brief 轻量类型列表，用于声明某个 C++ 类型具备哪些静态能力。
 */
template <typename... Types> struct type_list {};

namespace list_detail {

template <typename... Lists> struct concat_type_lists;

template <> struct concat_type_lists<> {
  using type = type_list<>;
};

template <typename... Types> struct concat_type_lists<type_list<Types...>> {
  using type = type_list<Types...>;
};

template <typename... Lhs, typename... Rhs, typename... Rest>
struct concat_type_lists<type_list<Lhs...>, type_list<Rhs...>, Rest...>
    : concat_type_lists<type_list<Lhs..., Rhs...>, Rest...> {};

template <typename... Lists>
using concat_type_lists_t = typename concat_type_lists<Lists...>::type;

} // namespace list_detail

/** @brief object 模块的 descriptor 静态能力贡献 tag。 */
struct object_static_capability_module {};

/** @brief serialization 模块的 descriptor 静态能力贡献 tag。 */
struct serialization_static_capability_module {};

/** @brief 用户或外部模块的 descriptor 静态能力贡献 tag。 */
struct custom_static_capability_module {};

/** @brief 指定模块对某个 C++ 类型贡献的 descriptor 级静态能力列表。 */
template <typename Module, typename T> struct module_static_capability_list {
  using type = type_list<>;
};

/**
 * @brief C++ 类型静态能力聚合列表。
 *
 * 各模块应优先特化自己的 `module_static_capability_list<Module, T>`，避免多个模块直接特化
 * 该聚合模板时互相覆盖。
 */
template <typename T> struct static_capability_list {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using type = list_detail::concat_type_lists_t<
      typename module_static_capability_list<object_static_capability_module, value_type>::type,
      typename module_static_capability_list<serialization_static_capability_module,
                                             value_type>::type,
      typename module_static_capability_list<custom_static_capability_module, value_type>::type>;
};

/** @brief 按 C++ 类型查询静态能力。 */
template <typename Capability, typename T> [[nodiscard]] const Capability* get() {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  return static_capability<capability_type, value_type>::get();
}

template <typename T, typename List> struct static_capability_entries;

template <typename T> struct static_capability_entries<T, type_list<>> {
  [[nodiscard]] static constexpr const capability_entry* data() noexcept { return nullptr; }
  [[nodiscard]] static constexpr std::size_t size() noexcept { return 0; }
};

template <typename T, typename... Capabilities>
struct static_capability_entries<T, type_list<Capabilities...>> {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;

  inline static const capability_entry values[sizeof...(Capabilities)] = {capability_entry{
      capability_key_of<std::remove_cv_t<std::remove_reference_t<Capabilities>>>(),
      get<std::remove_cv_t<std::remove_reference_t<Capabilities>>, value_type>()}...};

  [[nodiscard]] static const capability_entry* data() noexcept { return values; }
  [[nodiscard]] static constexpr std::size_t size() noexcept { return sizeof...(Capabilities); }
};

/** @brief 返回某个 C++ 类型声明的 descriptor 级静态能力记录。 */
template <typename T> [[nodiscard]] const capability_entry* static_capabilities_data() noexcept {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using list_type = typename static_capability_list<value_type>::type;
  return static_capability_entries<value_type, list_type>::data();
}

/** @brief 返回某个 C++ 类型声明的 descriptor 级静态能力数量。 */
template <typename T> [[nodiscard]] constexpr std::size_t static_capabilities_size() noexcept {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using list_type = typename static_capability_list<value_type>::type;
  return static_capability_entries<value_type, list_type>::size();
}

/** @brief 把一份稳定生命周期的能力指针挂到运行期索引。 */
template <typename Capability>
[[nodiscard]] error register_for(type_id id, const Capability* capability) {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  return detail::register_capability(id, capability_key_of<capability_type>(), capability);
}

/** @brief 把 C++ 类型的静态能力挂到运行期索引。 */
template <typename Capability, typename T> [[nodiscard]] error register_for() {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  const capability_type* capability = get<capability_type, value_type>();
  if (capability == nullptr) {
    return {error_domain::meta, errc::unsupported};
  }

  return register_for<capability_type>(detail::type_id_of_cpp_type<value_type>(), capability);
}

/** @brief 按运行期类型 id 查询已注册能力。 */
template <typename Capability> [[nodiscard]] const Capability* find(type_id id) noexcept {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  return static_cast<const capability_type*>(
      detail::find_capability(id, capability_key_of<capability_type>()));
}

/** @brief 判断运行期类型 id 是否已经注册指定能力。 */
template <typename Capability> [[nodiscard]] bool contains(type_id id) noexcept {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  return detail::contains_capability(id, capability_key_of<capability_type>());
}

/** @brief 返回指定能力在运行期索引中的数量。 */
template <typename Capability> [[nodiscard]] std::size_t size() noexcept {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  return detail::capability_count(capability_key_of<capability_type>());
}

} // namespace capabilities

} // namespace caors::meta

#endif
