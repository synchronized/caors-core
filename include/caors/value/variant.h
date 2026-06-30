// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_VARIANT_H_
#define CAORS_VALUE_VARIANT_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/export.h"

#include <cstdint>
#include <string_view>

namespace caors {

/** @brief 运行期数据值类型。 */
enum class variant_kind {
  null,
  boolean,
  signed_integer,
  unsigned_integer,
  floating,
  string,
  array,
  object,
};

class variant;
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
  [[nodiscard]] static variant object(object_type members);

  [[nodiscard]] variant_kind kind() const noexcept { return kind_; }
  [[nodiscard]] bool is_null() const noexcept { return kind_ == variant_kind::null; }
  [[nodiscard]] bool as_bool() const noexcept { return bool_value_; }
  [[nodiscard]] std::int64_t as_i64() const noexcept { return i64_value_; }
  [[nodiscard]] std::uint64_t as_u64() const noexcept { return u64_value_; }
  [[nodiscard]] double as_f64() const noexcept { return f64_value_; }
  [[nodiscard]] const string& as_string() const noexcept { return string_value_; }
  [[nodiscard]] const array_type& as_array() const noexcept { return array_value_; }
  [[nodiscard]] const object_type& as_object() const noexcept { return object_value_; }

  [[nodiscard]] array_type& mutable_array() noexcept { return array_value_; }
  [[nodiscard]] object_type& mutable_object() noexcept { return object_value_; }

private:
  variant_kind kind_ = variant_kind::null;
  bool bool_value_ = false;
  std::int64_t i64_value_ = 0;
  std::uint64_t u64_value_ = 0;
  double f64_value_ = 0.0;
  string string_value_{};
  array_type array_value_{};
  object_type object_value_{};
};

/** @brief 对象成员，保留字段名、字段 id 和字段值。 */
struct variant_member {
  string_name name{};
  std::uint32_t field_id = 0;
  variant value{};
};

/** @brief 兼容旧序列化 DOM 命名，后续新代码优先使用 `variant`。 */
using serialization_value_kind = variant_kind;
using serialization_value = variant;
using serialization_member = variant_member;

} // namespace caors

#endif
