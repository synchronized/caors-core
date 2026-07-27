// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CAPABILITY_TRAITS_H_
#define CAORS_META_CAPABILITY_TRAITS_H_

#include "caors/core/guid.h"
#include "caors/export.h"
#include "caors/meta/detail/canonical_type_name.h"
#include "caors/meta/identity_id.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace caors::meta {

class type;

namespace detail {

[[nodiscard]] CAORS_API const void* find_type_capability(const meta::type& type,
                                                         capability_key key) noexcept;
[[nodiscard]] CAORS_API capability_id get_or_create_capability_id(const char* canonical_name,
                                                                  std::size_t length) noexcept;

} // namespace detail

/** @brief 能力类型的可选稳定描述信息。 */
struct capability_info {
  capability_key key{};
  std::string_view name{};
  guid stable_guid{};
  std::uint32_t version = 0;

  [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(key); }
  [[nodiscard]] bool named() const noexcept { return !name.empty(); }
};

/** @brief 一条静态能力记录。 */
struct capability_entry {
  capability_key key{};
  const void* data = nullptr;

  [[nodiscard]] bool valid() const noexcept { return key && data != nullptr; }
};

/** @brief 返回能力类型在当前进程内的唯一 key。 */
template <typename Capability> [[nodiscard]] capability_key capability_key_of() noexcept {
  using capability_type = std::remove_cvref_t<Capability>;
  constexpr std::string_view canonical_name = detail::canonical_cpp_type_name<capability_type>();
  static const capability_key key =
      detail::get_or_create_capability_id(canonical_name.data(), canonical_name.size());
  return key;
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

template <typename Capability, typename = void> struct capability_data_type {
  using type = void;
};

template <typename Capability>
struct capability_data_type<
    Capability,
    std::void_t<typename std::remove_cv_t<std::remove_reference_t<Capability>>::data_type>> {
  using type = std::remove_cv_t<std::remove_reference_t<
      typename std::remove_cv_t<std::remove_reference_t<Capability>>::data_type>>;
};

template <typename Capability, typename = void> struct capability_view_type {
  using type = void;
};

template <typename Capability>
struct capability_view_type<
    Capability,
    std::void_t<typename std::remove_cv_t<std::remove_reference_t<Capability>>::view_type>> {
  using type = std::remove_cv_t<std::remove_reference_t<
      typename std::remove_cv_t<std::remove_reference_t<Capability>>::view_type>>;
};

template <typename Capability, typename = void> struct capability_ops_type {
  using type = void;
};

template <typename Capability>
struct capability_ops_type<
    Capability,
    std::void_t<typename std::remove_cv_t<std::remove_reference_t<Capability>>::ops_type>> {
  using type = std::remove_cv_t<std::remove_reference_t<
      typename std::remove_cv_t<std::remove_reference_t<Capability>>::ops_type>>;
};

template <typename Capability>
inline constexpr bool has_capability_data_type_v =
    !std::is_void_v<typename capability_data_type<Capability>::type>;

template <typename Capability>
inline constexpr bool has_capability_view_type_v =
    !std::is_void_v<typename capability_view_type<Capability>::type>;

template <typename Capability>
inline constexpr bool has_capability_ops_type_v =
    !std::is_void_v<typename capability_ops_type<Capability>::type>;

template <typename View> [[nodiscard]] bool capability_view_valid(const View& view) noexcept {
  if constexpr (requires(const View& value) { value.valid(); }) {
    return static_cast<bool>(view.valid());
  } else if constexpr (requires(const View& value) { static_cast<bool>(value); }) {
    return static_cast<bool>(view);
  } else {
    return true;
  }
}

template <typename View, typename Data>
[[nodiscard]] View make_data_capability_view(const meta::type& source, const Data* data) noexcept {
  if constexpr (requires { View{source, data}; }) {
    return View{source, data};
  } else if constexpr (requires { View{data}; }) {
    return View{data};
  } else {
    return View{};
  }
}

template <typename View, typename Ops, typename Data>
[[nodiscard]] View make_ops_data_capability_view(const meta::type& source, const Ops* ops,
                                                 const Data* data) noexcept {
  if constexpr (requires { View{source, ops, data}; }) {
    return View{source, ops, data};
  } else if constexpr (requires { View{source, data, ops}; }) {
    return View{source, data, ops};
  } else if constexpr (requires { View{ops, data}; }) {
    return View{ops, data};
  } else if constexpr (requires { View{data, ops}; }) {
    return View{data, ops};
  } else {
    return View{};
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

/**
 * @brief data + view 类型级 capability 的默认查询规则。
 *
 * `Capability` 提供 `data_type` 和 `view_type` 时，capability key 仍使用 `Capability`，
 * 存储的数据解释为 `data_type`，查询返回 `view_type`。
 */
template <typename Capability>
struct type_capability_traits<Capability,
                              std::enable_if_t<detail::has_capability_data_type_v<Capability> &&
                                               detail::has_capability_view_type_v<Capability> &&
                                               !detail::has_capability_ops_type_v<Capability>>> {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using data_type = typename detail::capability_data_type<capability_type>::type;
  using result_type = typename detail::capability_view_type<capability_type>::type;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return detail::make_data_capability_view<result_type>(source, get_raw(source));
  }

  [[nodiscard]] static const data_type* get_raw(const type& source) noexcept {
    return static_cast<const data_type*>(
        detail::find_type_capability(source, capability_key_of<capability_type>()));
  }

  [[nodiscard]] static bool has(const type& source) noexcept {
    return detail::capability_view_valid(get(source));
  }
};

/**
 * @brief ops + data + view 类型级 capability 的默认查询规则。
 *
 * `ops_type` 按自身 key 查询，`data_type` 按 capability key 查询，最终返回 `view_type`。
 */
template <typename Capability>
struct type_capability_traits<Capability,
                              std::enable_if_t<detail::has_capability_data_type_v<Capability> &&
                                               detail::has_capability_view_type_v<Capability> &&
                                               detail::has_capability_ops_type_v<Capability>>> {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using ops_type = typename detail::capability_ops_type<capability_type>::type;
  using data_type = typename detail::capability_data_type<capability_type>::type;
  using result_type = typename detail::capability_view_type<capability_type>::type;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return detail::make_ops_data_capability_view<result_type>(source, get_ops(source),
                                                              get_raw(source));
  }

  [[nodiscard]] static const data_type* get_raw(const type& source) noexcept {
    return static_cast<const data_type*>(
        detail::find_type_capability(source, capability_key_of<capability_type>()));
  }

  [[nodiscard]] static const ops_type* get_ops(const type& source) noexcept {
    return type_capability_traits<ops_type>::get_raw(source);
  }

  [[nodiscard]] static bool has(const type& source) noexcept {
    return detail::capability_view_valid(get(source));
  }
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
