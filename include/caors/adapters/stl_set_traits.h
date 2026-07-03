// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ADAPTERS_STL_SET_TRAITS_H_
#define CAORS_ADAPTERS_STL_SET_TRAITS_H_

#include "caors/core/containers.h"
#include "caors/traits/container.h"

#include <set>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace caors::traits {
/** @brief `std::unordered_set` / `caors::hash_set` 的集合容器反射适配。 */
template <typename T, typename Hash, typename Equal, typename Allocator>
struct set_traits<std::unordered_set<T, Hash, Equal, Allocator>> {
  static constexpr bool is_set = true;
  using element_type = std::remove_cv_t<T>;
  using set_type = std::unordered_set<T, Hash, Equal, Allocator>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    if constexpr (std::is_same_v<Allocator, allocator<T>>) {
      return "caors.hash_set";
    } else {
      return "std.unordered_set";
    }
  }

  [[nodiscard]] static std::size_t size(const set_type& values) noexcept { return values.size(); }

  template <typename Visitor> static void for_each(const set_type& values, Visitor&& visitor) {
    for (const element_type& value : values) {
      visitor(value);
    }
  }

  static void clear(set_type& values) { values.clear(); }

  [[nodiscard]] static bool insert(set_type& values, element_type value) {
    return values.emplace(std::move(value)).second;
  }
};

/** @brief `std::set` 的集合容器反射适配。 */
template <typename T, typename Compare, typename Allocator>
struct set_traits<std::set<T, Compare, Allocator>> {
  static constexpr bool is_set = true;
  using element_type = std::remove_cv_t<T>;
  using set_type = std::set<T, Compare, Allocator>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept { return "std.set"; }

  [[nodiscard]] static std::size_t size(const set_type& values) noexcept { return values.size(); }

  template <typename Visitor> static void for_each(const set_type& values, Visitor&& visitor) {
    for (const element_type& value : values) {
      visitor(value);
    }
  }

  static void clear(set_type& values) { values.clear(); }

  [[nodiscard]] static bool insert(set_type& values, element_type value) {
    return values.emplace(std::move(value)).second;
  }
};

} // namespace caors::traits

#endif
