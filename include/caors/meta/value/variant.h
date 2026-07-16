// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_VARIANT_H_
#define CAORS_META_VALUE_VARIANT_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/export.h"

#include <cstdint>
#include <string_view>
#include <utility>

namespace caors::meta {

/** @brief 运行期数据值类型。 */
enum class variant_kind {
  null,
  boolean,
  signed_integer,
  unsigned_integer,
  floating,
  string,
  array,
  dictionary,
  object,
};

class variant;
struct variant_entry;
struct variant_member;

/**
 * @brief 数据 DOM 中间对象。
 *
 * `variant` 保存已经脱离 C++ 精确类型的运行期数据，适合序列化、配置和跨格式转换使用。
 * 需要保留真实 C++ 类型的场景应使用 `any`。
 */
class CAORS_API variant final {
public:
  using array_type = vector<variant>;
  using dictionary_type = vector<variant_entry>;
  using object_type = vector<variant_member>;

  variant();
  ~variant();

  variant(const variant& other);
  variant(variant&& other) noexcept;
  variant& operator=(const variant& other);
  variant& operator=(variant&& other) noexcept;

  [[nodiscard]] static variant null();
  [[nodiscard]] static variant from(bool value);
  [[nodiscard]] static variant from(std::int64_t value);
  [[nodiscard]] static variant from(std::uint64_t value);
  [[nodiscard]] static variant from(double value);
  [[nodiscard]] static variant from(std::string_view value);
  [[nodiscard]] static variant array(array_type values);
  [[nodiscard]] static variant dictionary(dictionary_type entries);
  [[nodiscard]] static variant object(object_type members);

  [[nodiscard]] variant_kind kind() const noexcept { return kind_; }
  [[nodiscard]] bool is_null() const noexcept { return kind_ == variant_kind::null; }
  [[nodiscard]] bool is_bool() const noexcept { return kind_ == variant_kind::boolean; }
  [[nodiscard]] bool is_i64() const noexcept { return kind_ == variant_kind::signed_integer; }
  [[nodiscard]] bool is_u64() const noexcept { return kind_ == variant_kind::unsigned_integer; }
  [[nodiscard]] bool is_f64() const noexcept { return kind_ == variant_kind::floating; }
  [[nodiscard]] bool is_string() const noexcept { return kind_ == variant_kind::string; }
  [[nodiscard]] bool is_array() const noexcept { return kind_ == variant_kind::array; }
  [[nodiscard]] bool is_dictionary() const noexcept { return kind_ == variant_kind::dictionary; }
  [[nodiscard]] bool is_object() const noexcept { return kind_ == variant_kind::object; }

  [[nodiscard]] bool as_bool() const noexcept { return bool_value_; }
  [[nodiscard]] std::int64_t as_i64() const noexcept { return i64_value_; }
  [[nodiscard]] std::uint64_t as_u64() const noexcept { return u64_value_; }
  [[nodiscard]] double as_f64() const noexcept { return f64_value_; }
  [[nodiscard]] const string& as_string() const noexcept { return string_value_; }
  [[nodiscard]] const array_type& as_array() const noexcept { return array_value_; }
  [[nodiscard]] const dictionary_type& as_dictionary() const noexcept { return dictionary_value_; }
  [[nodiscard]] const object_type& as_object() const noexcept { return object_value_; }

  [[nodiscard]] const bool* try_as_bool() const noexcept {
    return is_bool() ? &bool_value_ : nullptr;
  }
  [[nodiscard]] const std::int64_t* try_as_i64() const noexcept {
    return is_i64() ? &i64_value_ : nullptr;
  }
  [[nodiscard]] const std::uint64_t* try_as_u64() const noexcept {
    return is_u64() ? &u64_value_ : nullptr;
  }
  [[nodiscard]] const double* try_as_f64() const noexcept {
    return is_f64() ? &f64_value_ : nullptr;
  }
  [[nodiscard]] const string* try_as_string() const noexcept {
    return is_string() ? &string_value_ : nullptr;
  }
  [[nodiscard]] const array_type* try_as_array() const noexcept {
    return is_array() ? &array_value_ : nullptr;
  }
  [[nodiscard]] const dictionary_type* try_as_dictionary() const noexcept {
    return is_dictionary() ? &dictionary_value_ : nullptr;
  }
  [[nodiscard]] const object_type* try_as_object() const noexcept {
    return is_object() ? &object_value_ : nullptr;
  }

  [[nodiscard]] array_type& mutable_array() noexcept { return array_value_; }
  [[nodiscard]] dictionary_type& mutable_dictionary() noexcept { return dictionary_value_; }
  [[nodiscard]] object_type& mutable_object() noexcept { return object_value_; }
  [[nodiscard]] array_type* try_mutable_array() noexcept {
    return is_array() ? &array_value_ : nullptr;
  }
  [[nodiscard]] dictionary_type* try_mutable_dictionary() noexcept {
    return is_dictionary() ? &dictionary_value_ : nullptr;
  }
  [[nodiscard]] object_type* try_mutable_object() noexcept {
    return is_object() ? &object_value_ : nullptr;
  }

  bool push_back(variant value);
  bool add_entry(variant key, variant value);
  bool add_member(string_name name, std::uint32_t field_id, variant value);
  /** @brief 按名称查找对象成员完整描述。 */
  [[nodiscard]] const variant_member* find_member(string_name name) const noexcept;
  /** @brief 按字段 id 查找对象成员完整描述。 */
  [[nodiscard]] const variant_member* find_member(std::uint32_t field_id) const noexcept;
  /** @brief 按名称查找对象成员完整描述。 */
  [[nodiscard]] variant_member* find_member(string_name name) noexcept;
  /** @brief 按字段 id 查找对象成员完整描述。 */
  [[nodiscard]] variant_member* find_member(std::uint32_t field_id) noexcept;
  /** @brief 按名称查找对象成员值。 */
  [[nodiscard]] const variant* find_member_variant(string_name name) const noexcept;
  /** @brief 按字段 id 查找对象成员值。 */
  [[nodiscard]] const variant* find_member_variant(std::uint32_t field_id) const noexcept;
  /** @brief 按名称查找对象成员值。 */
  [[nodiscard]] variant* find_member_variant(string_name name) noexcept;
  /** @brief 按字段 id 查找对象成员值。 */
  [[nodiscard]] variant* find_member_variant(std::uint32_t field_id) noexcept;

private:
  variant_kind kind_ = variant_kind::null;
  bool bool_value_ = false;
  std::int64_t i64_value_ = 0;
  std::uint64_t u64_value_ = 0;
  double f64_value_ = 0.0;
  string string_value_{};
  array_type array_value_{};
  dictionary_type dictionary_value_{};
  object_type object_value_{};
};

/** @brief 字典条目，key 和 value 都保存为 DOM 值。 */
struct variant_entry {
  variant key{};
  variant value{};
};

/** @brief 对象成员，保留字段名、字段 id 和字段值。 */
struct variant_member {
  string_name name{};
  std::uint32_t field_id = 0;
  variant value{};
};

} // namespace caors::meta

namespace caors {

using meta::variant;

} // namespace caors

#endif
