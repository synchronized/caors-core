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

/** @brief 按 C++ 类型查询静态能力。 */
template <typename Capability, typename T> [[nodiscard]] const Capability* get() {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  return static_capability<capability_type, value_type>::get();
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
