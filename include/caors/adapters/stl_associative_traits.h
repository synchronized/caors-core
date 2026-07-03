// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ADAPTERS_STL_ASSOCIATIVE_TRAITS_H_
#define CAORS_ADAPTERS_STL_ASSOCIATIVE_TRAITS_H_

#include "caors/core/containers.h"
#include "caors/traits/container.h"

#include <map>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace caors::traits {
/** @brief `std::unordered_map` / `caors::hash_map` 的关联容器反射适配。 */
template <typename K, typename V, typename Hash, typename Equal, typename Allocator>
struct associative_traits<std::unordered_map<K, V, Hash, Equal, Allocator>> {
  static constexpr bool is_associative = true;
  using key_type = std::remove_cv_t<K>;
  using mapped_type = std::remove_cv_t<V>;
  using map_type = std::unordered_map<K, V, Hash, Equal, Allocator>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    if constexpr (std::is_same_v<Allocator, allocator<std::pair<const K, V>>>) {
      return "caors.hash_map";
    } else {
      return "std.unordered_map";
    }
  }

  [[nodiscard]] static std::size_t size(const map_type& values) noexcept { return values.size(); }

  template <typename Visitor> static void for_each(const map_type& values, Visitor&& visitor) {
    for (const auto& [key, value] : values) {
      visitor(key, value);
    }
  }

  static void clear(map_type& values) { values.clear(); }

  [[nodiscard]] static bool insert(map_type& values, key_type key, mapped_type value) {
    return values.emplace(std::move(key), std::move(value)).second;
  }
};

/** @brief `std::map` 的关联容器反射适配。 */
template <typename K, typename V, typename Compare, typename Allocator>
struct associative_traits<std::map<K, V, Compare, Allocator>> {
  static constexpr bool is_associative = true;
  using key_type = std::remove_cv_t<K>;
  using mapped_type = std::remove_cv_t<V>;
  using map_type = std::map<K, V, Compare, Allocator>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept { return "std.map"; }

  [[nodiscard]] static std::size_t size(const map_type& values) noexcept { return values.size(); }

  template <typename Visitor> static void for_each(const map_type& values, Visitor&& visitor) {
    for (const auto& [key, value] : values) {
      visitor(key, value);
    }
  }

  static void clear(map_type& values) { values.clear(); }

  [[nodiscard]] static bool insert(map_type& values, key_type key, mapped_type value) {
    return values.emplace(std::move(key), std::move(value)).second;
  }
};

} // namespace caors::traits

#endif
