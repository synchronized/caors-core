// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ADAPTERS_STL_TRAITS_H_
#define CAORS_ADAPTERS_STL_TRAITS_H_

#include "caors/core/containers.h"
#include "caors/meta/container_traits.h"

#include <array>
#include <deque>
#include <iterator>
#include <list>
#include <map>
#include <set>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace caors {

/** @brief `std::vector` / `caors::vector` 的顺序容器反射适配。 */
template <typename T, typename Allocator> struct sequence_traits<std::vector<T, Allocator>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    if constexpr (std::is_same_v<Allocator, allocator<T>>) {
      return "caors.vector";
    } else {
      return "std.vector";
    }
  }

  [[nodiscard]] static std::size_t size(const std::vector<T, Allocator>& values) noexcept {
    return values.size();
  }

  [[nodiscard]] static const element_type* element(const std::vector<T, Allocator>& values,
                                                   std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(std::vector<T, Allocator>& values,
                                                     std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  static void clear(std::vector<T, Allocator>& values) { values.clear(); }

  [[nodiscard]] static bool resize(std::vector<T, Allocator>& values, std::size_t size) {
    if constexpr (std::is_default_constructible_v<T>) {
      values.resize(size);
      return true;
    } else {
      return false;
    }
  }

  [[nodiscard]] static bool append(std::vector<T, Allocator>& values, const element_type& value) {
    if constexpr (std::is_copy_constructible_v<T>) {
      values.push_back(value);
      return true;
    } else {
      return false;
    }
  }
};

/** @brief `std::array` 的固定长度顺序容器反射适配。 */
template <typename T, std::size_t Size> struct sequence_traits<std::array<T, Size>> {
  static constexpr bool is_sequence = true;
  static constexpr std::size_t fixed_size = Size;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "std.array";
  }

  [[nodiscard]] static constexpr std::size_t size(const std::array<T, Size>&) noexcept {
    return Size;
  }

  [[nodiscard]] static const element_type* element(const std::array<T, Size>& values,
                                                   std::size_t index) noexcept {
    return index < Size ? &values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(std::array<T, Size>& values,
                                                     std::size_t index) noexcept {
    return index < Size ? &values[index] : nullptr;
  }

  static void clear(std::array<T, Size>&) noexcept {}

  [[nodiscard]] static constexpr bool resize(std::array<T, Size>&, std::size_t size) noexcept {
    return size == Size;
  }

  [[nodiscard]] static constexpr bool append(std::array<T, Size>&, const element_type&) noexcept {
    return false;
  }
};

/** @brief `std::deque` 的顺序容器反射适配。 */
template <typename T, typename Allocator> struct sequence_traits<std::deque<T, Allocator>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "std.deque";
  }

  [[nodiscard]] static std::size_t size(const std::deque<T, Allocator>& values) noexcept {
    return values.size();
  }

  [[nodiscard]] static const element_type* element(const std::deque<T, Allocator>& values,
                                                   std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(std::deque<T, Allocator>& values,
                                                     std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  static void clear(std::deque<T, Allocator>& values) { values.clear(); }

  [[nodiscard]] static bool resize(std::deque<T, Allocator>& values, std::size_t size) {
    if constexpr (std::is_default_constructible_v<T>) {
      values.resize(size);
      return true;
    } else {
      return false;
    }
  }

  [[nodiscard]] static bool append(std::deque<T, Allocator>& values, const element_type& value) {
    if constexpr (std::is_copy_constructible_v<T>) {
      values.push_back(value);
      return true;
    } else {
      return false;
    }
  }
};

/** @brief `std::list` / `caors::list` 的顺序容器反射适配。 */
template <typename T, typename Allocator> struct sequence_traits<std::list<T, Allocator>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;
  using list_type = std::list<T, Allocator>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    if constexpr (std::is_same_v<Allocator, allocator<T>>) {
      return "caors.list";
    } else {
      return "std.list";
    }
  }

  [[nodiscard]] static std::size_t size(const list_type& values) noexcept { return values.size(); }

  [[nodiscard]] static const element_type* element(const list_type& values,
                                                   std::size_t index) noexcept {
    if (index >= values.size()) {
      return nullptr;
    }

    auto iterator = values.begin();
    std::advance(iterator, static_cast<typename list_type::difference_type>(index));
    return &*iterator;
  }

  [[nodiscard]] static element_type* mutable_element(list_type& values,
                                                     std::size_t index) noexcept {
    if (index >= values.size()) {
      return nullptr;
    }

    auto iterator = values.begin();
    std::advance(iterator, static_cast<typename list_type::difference_type>(index));
    return &*iterator;
  }

  static void clear(list_type& values) { values.clear(); }

  [[nodiscard]] static bool resize(list_type& values, std::size_t size) {
    if constexpr (std::is_default_constructible_v<T>) {
      values.resize(size);
      return true;
    } else {
      return false;
    }
  }

  [[nodiscard]] static bool append(list_type& values, const element_type& value) {
    if constexpr (std::is_copy_constructible_v<T>) {
      values.push_back(value);
      return true;
    } else {
      return false;
    }
  }
};

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

/** @brief `std::pair` 的二元组反射适配。 */
template <typename First, typename Second> struct pair_traits<std::pair<First, Second>> {
  static constexpr bool is_pair = true;
  using first_type = std::remove_cv_t<First>;
  using second_type = std::remove_cv_t<Second>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept { return "std.pair"; }

  [[nodiscard]] static const first_type* first(const std::pair<First, Second>& values) noexcept {
    return &values.first;
  }

  [[nodiscard]] static first_type* mutable_first(std::pair<First, Second>& values) noexcept {
    if constexpr (std::is_const_v<First>) {
      (void)values;
      return nullptr;
    } else {
      return &values.first;
    }
  }

  [[nodiscard]] static const second_type* second(const std::pair<First, Second>& values) noexcept {
    return &values.second;
  }

  [[nodiscard]] static second_type* mutable_second(std::pair<First, Second>& values) noexcept {
    if constexpr (std::is_const_v<Second>) {
      (void)values;
      return nullptr;
    } else {
      return &values.second;
    }
  }
};

} // namespace caors

#endif
