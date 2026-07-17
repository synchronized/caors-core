// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CAPABILITY_TRAITS_H_
#define CAORS_META_CAPABILITY_TRAITS_H_

#include "caors/core/guid.h"
#include "caors/meta/detail/capability_registry.h"

#include <cstdint>
#include <string_view>
#include <type_traits>

namespace caors::meta {

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

namespace detail {

/** @brief 判断 ops 函数表是否可用；有 `valid()` 时优先使用它。 */
template <typename Ops> [[nodiscard]] bool capability_ops_valid(const Ops* ops) noexcept {
  if (ops == nullptr) {
    return false;
  }

  if constexpr (requires(const Ops& value) { value.valid(); }) {
    return static_cast<bool>(ops->valid());
  } else {
    return true;
  }
}

} // namespace detail

/**
 * @brief 运行期实例能力视图的绑定扩展点。
 *
 * `Capability` 是传给 `any_ref::get_capability<Capability>()` 的查询类型。内置 capability 通常返回
 * 绑定到具体 `any_ref` 后可直接调用的轻量 view，例如 `meta::sequence_view` 或
 * `objects::ptr_view`。默认表示该 capability 不支持实例绑定查询。
 */
template <typename Capability, typename = void> struct capability_view_traits {
  using result_type = std::remove_cv_t<std::remove_reference_t<Capability>>;

  static constexpr bool supported = false;
};

/**
 * @brief 类型级 capability 查询扩展点。
 *
 * 默认情况下，`meta::type::get_capability<T>()` 返回 `const T*`，用于用户自定义
 * concept table 或模块静态能力。内置公开 facade 可以特化该模板，让查询返回可直接调用的轻量对象。
 */
template <typename Capability, typename = void> struct type_capability_traits {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using result_type = const capability_type*;

  [[nodiscard]] static result_type get(const type& source) noexcept { return get_raw(source); }

  [[nodiscard]] static result_type get_raw(const type& source) noexcept {
    return static_cast<const capability_type*>(
        detail::find_type_capability(source, capability_key_of<capability_type>()));
  }

  [[nodiscard]] static bool has(const type& source) noexcept { return get_raw(source) != nullptr; }
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

} // namespace caors::meta

#endif
