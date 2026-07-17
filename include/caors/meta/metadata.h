// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_METADATA_H_
#define CAORS_META_METADATA_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/memory/resource.h"
#include "caors/meta/value/variant.h"

#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta {

/** @brief 通用 metadata 键。 */
using metadata_key = core::string_name;

/** @brief 通用 metadata 值。 */
using metadata_value = variant;

/** @brief 通用 metadata 表。 */
using metadata_map = core::hash_map<metadata_key, metadata_value>;

/** @brief 类型级 metadata 组件。 */
struct type_metadata {
  explicit type_metadata(memory_resource* resource = nullptr)
      : values(core::make_hash_map<metadata_key, metadata_value>(
            resource != nullptr ? resource : &memory::library_resource())) {}

  metadata_map values;
};

/** @brief 创建 metadata 表。 */
[[nodiscard]] inline metadata_map make_metadata_map(memory_resource* resource = nullptr) {
  return core::make_hash_map<metadata_key, metadata_value>(
      resource != nullptr ? resource : &memory::library_resource());
}

/** @brief 查询 metadata 值。 */
[[nodiscard]] inline const metadata_value* find_metadata(const metadata_map& metadata,
                                                         metadata_key key) noexcept {
  const auto position = metadata.find(key);
  return position != metadata.end() ? &position->second : nullptr;
}

/** @brief 写入或覆盖 metadata 值。 */
inline void set_metadata(metadata_map& metadata, metadata_key key, metadata_value value) {
  metadata.insert_or_assign(key, std::move(value));
}

/** @brief 转换 metadata 值。 */
[[nodiscard]] inline metadata_value make_metadata_value(const metadata_value& value) {
  return value;
}

/** @brief 转换 metadata 值。 */
[[nodiscard]] inline metadata_value make_metadata_value(metadata_value&& value) noexcept {
  return std::move(value);
}

/** @brief 转换 metadata 值。 */
[[nodiscard]] inline metadata_value make_metadata_value(bool value) {
  return metadata_value::from(value);
}

/** @brief 转换 metadata 值。 */
template <typename T>
  requires(std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>> &&
           std::is_signed_v<std::remove_cv_t<std::remove_reference_t<T>>> &&
           !std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, bool>)
[[nodiscard]] metadata_value make_metadata_value(T value) {
  return metadata_value::from(static_cast<std::int64_t>(value));
}

/** @brief 转换 metadata 值。 */
template <typename T>
  requires(std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>> &&
           std::is_unsigned_v<std::remove_cv_t<std::remove_reference_t<T>>>)
[[nodiscard]] metadata_value make_metadata_value(T value) {
  return metadata_value::from(static_cast<std::uint64_t>(value));
}

/** @brief 转换 metadata 值。 */
template <typename T>
  requires(std::is_floating_point_v<std::remove_cv_t<std::remove_reference_t<T>>>)
[[nodiscard]] metadata_value make_metadata_value(T value) {
  return metadata_value::from(static_cast<double>(value));
}

/** @brief 转换 metadata 值。 */
[[nodiscard]] inline metadata_value make_metadata_value(std::string_view value) {
  return metadata_value::from(value);
}

/** @brief 转换 metadata 值。 */
[[nodiscard]] inline metadata_value make_metadata_value(const char* value) {
  return metadata_value::from(value != nullptr ? std::string_view{value} : std::string_view{});
}

/** @brief 转换 metadata 值。 */
[[nodiscard]] inline metadata_value make_metadata_value(core::string_name value) {
  return metadata_value::from(value.view());
}

/** @brief 创建字符串数组 metadata。 */
[[nodiscard]] inline metadata_value
make_metadata_string_array(std::initializer_list<std::string_view> values,
                           memory_resource* resource = nullptr) {
  metadata_value::array_type array{typename metadata_value::array_type::allocator_type{
      resource != nullptr ? resource : &memory::library_resource()}};
  array.reserve(values.size());
  for (std::string_view value : values) {
    array.push_back(metadata_value::from(value));
  }
  return metadata_value::array(std::move(array));
}

} // namespace caors::meta

namespace caors::meta::registration {

/** @brief 一条待写入的 metadata。 */
struct metadata_entry {
  metadata_key key{};
  metadata_value value{};
};

/** @brief 创建一条 metadata 写入项。 */
[[nodiscard]] inline metadata_entry make_metadata_entry(metadata_key key, metadata_value value) {
  return metadata_entry{key, std::move(value)};
}

/** @brief 创建一条 metadata 写入项。 */
template <typename Value>
[[nodiscard]] metadata_entry metadata(std::string_view key, Value&& value) {
  return make_metadata_entry(metadata_key{key}, make_metadata_value(std::forward<Value>(value)));
}

/** @brief 创建一条 metadata 写入项。 */
template <typename Value> [[nodiscard]] metadata_entry metadata(metadata_key key, Value&& value) {
  return make_metadata_entry(key, make_metadata_value(std::forward<Value>(value)));
}

} // namespace caors::meta::registration

#endif
