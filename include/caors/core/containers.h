// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_CORE_CONTAINERS_H_
#define CAORS_CORE_CONTAINERS_H_

#include "caors/memory/resource.h"

#include <functional>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace caors::core {

/** @brief 使用 CAORS allocator 的动态数组别名。 */
template <typename T> using vector = std::vector<T, allocator<T>>;

/** @brief 使用 CAORS allocator 的双向链表别名。 */
template <typename T> using list = std::list<T, allocator<T>>;

/** @brief 使用 CAORS allocator 的字符串别名。 */
using string = std::basic_string<char, std::char_traits<char>, allocator<char>>;

/** @brief 使用 CAORS allocator 的哈希表别名。 */
template <typename K, typename V, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
using hash_map = std::unordered_map<K, V, Hash, Equal, allocator<std::pair<const K, V>>>;

/** @brief 使用 CAORS allocator 的哈希集合别名。 */
template <typename T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
using hash_set = std::unordered_set<T, Hash, Equal, allocator<T>>;

/** @brief 使用指定 CAORS 资源创建字符串。 */
[[nodiscard]] inline string make_string(memory_resource* resource = nullptr) {
  return string{allocator<char>{resource}};
}

/** @brief 使用指定 CAORS 资源创建动态数组。 */
template <typename T> [[nodiscard]] vector<T> make_vector(memory_resource* resource = nullptr) {
  return vector<T>{allocator<T>{resource}};
}

/** @brief 使用指定 CAORS 资源创建双向链表。 */
template <typename T> [[nodiscard]] list<T> make_list(memory_resource* resource = nullptr) {
  return list<T>{allocator<T>{resource}};
}

/** @brief 使用指定 CAORS 资源创建哈希表。 */
template <typename K, typename V, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
[[nodiscard]] hash_map<K, V, Hash, Equal> make_hash_map(memory_resource* resource = nullptr) {
  return hash_map<K, V, Hash, Equal>{
      typename hash_map<K, V, Hash, Equal>::allocator_type{resource}};
}

/** @brief 使用指定 CAORS 资源创建哈希集合。 */
template <typename T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
[[nodiscard]] hash_set<T, Hash, Equal> make_hash_set(memory_resource* resource = nullptr) {
  return hash_set<T, Hash, Equal>{typename hash_set<T, Hash, Equal>::allocator_type{resource}};
}

} // namespace caors::core

namespace caors {

template <typename T> using vector = core::vector<T>;
template <typename T> using list = core::list<T>;
using string = core::string;
template <typename K, typename V, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
using hash_map = core::hash_map<K, V, Hash, Equal>;
template <typename T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
using hash_set = core::hash_set<T, Hash, Equal>;

using core::make_hash_map;
using core::make_hash_set;
using core::make_list;
using core::make_string;
using core::make_vector;

} // namespace caors

#endif
