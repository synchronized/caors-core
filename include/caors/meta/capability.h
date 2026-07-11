// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CAPABILITY_H_
#define CAORS_META_CAPABILITY_H_

#include "caors/core/guid.h"
#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta/detail/capability_registry.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta {

class type;

/** @brief 运行期能力类型的唯一 key。 */
using capability_key = const void*;

/** @brief 能力类型的可选稳定描述信息。 */
struct capability_info {
  capability_key key = nullptr;
  std::string_view name{};
  guid stable_guid{};
  std::uint32_t version = 0;

  [[nodiscard]] bool valid() const noexcept { return key != nullptr; }
  [[nodiscard]] bool named() const noexcept { return !name.empty(); }
};

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

/**
 * @brief 能力类型的描述信息扩展点。
 *
 * `capability_key` 仍是当前进程内的快速查询 key；`name`、`stable_guid` 和 `version` 用于诊断、
 * 插件协议和未来跨模块协商。默认表示该能力没有稳定描述信息。
 */
template <typename Capability> struct capability_traits {
  static constexpr std::string_view name{};
  static constexpr guid stable_guid{};
  static constexpr std::uint32_t version = 0;
};

/** @brief 返回能力类型的查询 key 和可选稳定描述信息。 */
template <typename Capability> [[nodiscard]] capability_info capability_info_of() noexcept {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  return capability_info{
      capability_key_of<capability_type>(),
      capability_traits<capability_type>::name,
      capability_traits<capability_type>::stable_guid,
      capability_traits<capability_type>::version,
  };
}

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

template <typename... Lists> using concat_type_lists_t = typename concat_type_lists<Lists...>::type;

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

/** @brief 外部模块可为指定 C++ 类型追加的静态能力模块 tag 列表。 */
template <typename T> struct extension_static_capability_modules {
  using type = type_list<>;
};

/** @brief 指定 C++ 类型参与聚合的静态能力模块 tag 列表。 */
template <typename T> struct static_capability_modules {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using type = list_detail::concat_type_lists_t<
      type_list<object_static_capability_module, serialization_static_capability_module,
                custom_static_capability_module>,
      typename extension_static_capability_modules<value_type>::type>;
};

template <typename T, typename ModuleList> struct module_static_capability_lists;

template <typename T, typename... Modules>
struct module_static_capability_lists<T, type_list<Modules...>> {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using type = list_detail::concat_type_lists_t<
      typename module_static_capability_list<Modules, value_type>::type...>;
};

/**
 * @brief C++ 类型静态能力聚合列表。
 *
 * 内建模块由 `static_capability_modules<T>` 默认聚合；外部模块应定义自己的 module tag，并通过
 * `extension_static_capability_modules<T>` 追加到模块列表，避免多个模块都覆盖同一个 custom 槽位。
 */
template <typename T> struct static_capability_list {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using module_list = typename static_capability_modules<value_type>::type;
  using type = typename module_static_capability_lists<value_type, module_list>::type;
};

/** @brief 按 C++ 类型查询静态能力。 */
template <typename Capability, typename T> [[nodiscard]] const Capability* get() {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  return static_capability<capability_type, value_type>::get();
}

} // namespace capabilities

namespace detail {

/** @brief 把一份稳定生命周期的能力指针挂到运行期索引。内部入口。 */
template <typename Capability>
[[nodiscard]] error register_capability_for(type_id id, const Capability* capability) {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  return register_capability(id, capability_key_of<capability_type>(), capability);
}

/** @brief 按 C++ 类型把一份稳定生命周期的能力指针挂到运行期索引。内部入口。 */
template <typename Capability, typename T>
[[nodiscard]] error register_capability_for_type(const Capability* capability) {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  return register_capability_for<capability_type>(type_id_of_cpp_type<value_type>(), capability);
}

/** @brief 按 C++ 类型把一份运行期索引拥有的能力对象挂到运行期索引。内部入口。 */
template <typename Capability, typename T>
[[nodiscard]] error register_owned_capability_for_type(Capability capability) {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  auto owned = make_owned_capability<capability_type>(capability_type{std::move(capability)});
  return register_owned_capability(type_id_of_cpp_type<value_type>(),
                                   capability_key_of<capability_type>(), std::move(owned));
}

/** @brief 把 C++ 类型的静态能力挂到运行期索引。内部入口。 */
template <typename Capability, typename T> [[nodiscard]] error register_static_capability_for() {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  const capability_type* capability = capabilities::get<capability_type, value_type>();
  if (capability == nullptr) {
    return {error_domain::meta, errc::unsupported};
  }

  return register_capability_for_type<capability_type, value_type>(capability);
}

} // namespace detail

namespace capabilities {

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
