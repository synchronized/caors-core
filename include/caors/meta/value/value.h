// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_VALUE_H_
#define CAORS_META_VALUE_VALUE_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/meta/argument.h"
#include "caors/meta/object.h"
#include "caors/meta/value/any_ref.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace caors::meta {

/**
 * @brief 高频运行期动态值分类。
 *
 * `value` 面向脚本、编辑器、消息和动态方法调用等运行期热路径；需要可持久化数据树时使用
 * `meta::variant`，需要保留精确 C++ 类型拥有值时使用 `meta::any`，需要引用外部 C++ 对象时使用
 * `meta::any_ref`。
 */
enum class value_kind {
  null,
  boolean,
  signed_integer,
  unsigned_integer,
  floating,
  string_name,
  string,
  object,
  reference,
  any,
};

/**
 * @brief 轻量运行期动态值。
 *
 * `value` 内联保存常见标量、`string_name` 和 `meta::object*`，并用 `any_ref` 表达非拥有 C++
 * 引用、用 `any` 兜底表达拥有的精确 C++ 类型值。它不替代序列化 DOM，也不保证所有成员都无分配。
 */
class value final {
public:
  value() noexcept = default;
  value(std::nullptr_t) noexcept {}
  value(bool source) noexcept : storage_(source) {}

  template <typename T>
    requires(std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>> &&
             !std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, bool>)
  value(T source) noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_signed_v<type>) {
      storage_ = static_cast<std::int64_t>(source);
    } else {
      storage_ = static_cast<std::uint64_t>(source);
    }
  }

  template <typename T>
    requires(std::is_floating_point_v<std::remove_cv_t<std::remove_reference_t<T>>>)
  value(T source) noexcept : storage_(static_cast<double>(source)) {}

  value(core::string_name source) noexcept : storage_(source) {}
  value(std::string_view source) : storage_(core::string{source}) {}
  value(const char* source)
      : storage_(source != nullptr ? storage_type{core::string{source}} : storage_type{}) {}
  value(const core::string& source) : storage_(source) {}
  value(core::string&& source) noexcept : storage_(std::move(source)) {}
  explicit value(meta::object* source) noexcept : storage_(source) {}
  explicit value(meta::any_ref source) noexcept : storage_(source) {}
  explicit value(meta::any source) noexcept : storage_(std::move(source)) {}

  [[nodiscard]] static value null() noexcept { return {}; }
  [[nodiscard]] static value from_object(meta::object* source) noexcept { return value{source}; }
  [[nodiscard]] static value from_ref(meta::any_ref source) noexcept { return value{source}; }
  [[nodiscard]] static value from_any(meta::any source) noexcept {
    return value{std::move(source)};
  }

  template <typename T> [[nodiscard]] static value from_ref(T& source) {
    return value{meta::any_ref::from(source)};
  }

  template <typename T> [[nodiscard]] static value from_any(const T& source) {
    return value{meta::any::from(source)};
  }

  [[nodiscard]] value_kind kind() const noexcept {
    switch (storage_.index()) {
    case 1:
      return value_kind::boolean;
    case 2:
      return value_kind::signed_integer;
    case 3:
      return value_kind::unsigned_integer;
    case 4:
      return value_kind::floating;
    case 5:
      return value_kind::string_name;
    case 6:
      return value_kind::string;
    case 7:
      return value_kind::object;
    case 8:
      return value_kind::reference;
    case 9:
      return value_kind::any;
    default:
      return value_kind::null;
    }
  }

  [[nodiscard]] bool is_null() const noexcept { return kind() == value_kind::null; }
  [[nodiscard]] bool is_bool() const noexcept { return kind() == value_kind::boolean; }
  [[nodiscard]] bool is_i64() const noexcept { return kind() == value_kind::signed_integer; }
  [[nodiscard]] bool is_u64() const noexcept { return kind() == value_kind::unsigned_integer; }
  [[nodiscard]] bool is_f64() const noexcept { return kind() == value_kind::floating; }
  [[nodiscard]] bool is_string_name() const noexcept { return kind() == value_kind::string_name; }
  [[nodiscard]] bool is_string() const noexcept { return kind() == value_kind::string; }
  [[nodiscard]] bool is_object() const noexcept { return kind() == value_kind::object; }
  [[nodiscard]] bool is_ref() const noexcept { return kind() == value_kind::reference; }
  [[nodiscard]] bool is_any() const noexcept { return kind() == value_kind::any; }
  [[nodiscard]] bool is_number() const noexcept { return is_i64() || is_u64() || is_f64(); }
  [[nodiscard]] bool is_text() const noexcept { return is_string_name() || is_string(); }

  [[nodiscard]] bool as_bool() const noexcept {
    const bool* value = try_as_bool();
    return value != nullptr ? *value : false;
  }

  [[nodiscard]] std::int64_t as_i64() const noexcept {
    const std::int64_t* value = try_as_i64();
    return value != nullptr ? *value : 0;
  }

  [[nodiscard]] std::uint64_t as_u64() const noexcept {
    const std::uint64_t* value = try_as_u64();
    return value != nullptr ? *value : 0;
  }

  [[nodiscard]] double as_f64() const noexcept {
    const double* value = try_as_f64();
    return value != nullptr ? *value : 0.0;
  }

  [[nodiscard]] core::string_name as_string_name() const noexcept {
    const core::string_name* value = try_as_string_name();
    return value != nullptr ? *value : core::string_name{};
  }

  [[nodiscard]] meta::object* as_object() const noexcept {
    const auto* value = std::get_if<meta::object*>(&storage_);
    return value != nullptr ? *value : nullptr;
  }

  [[nodiscard]] meta::any_ref as_ref() const noexcept {
    const meta::any_ref* value = try_as_ref();
    return value != nullptr ? *value : meta::any_ref{};
  }

  [[nodiscard]] const bool* try_as_bool() const noexcept { return std::get_if<bool>(&storage_); }
  [[nodiscard]] const std::int64_t* try_as_i64() const noexcept {
    return std::get_if<std::int64_t>(&storage_);
  }
  [[nodiscard]] const std::uint64_t* try_as_u64() const noexcept {
    return std::get_if<std::uint64_t>(&storage_);
  }
  [[nodiscard]] const double* try_as_f64() const noexcept { return std::get_if<double>(&storage_); }
  [[nodiscard]] const core::string_name* try_as_string_name() const noexcept {
    return std::get_if<core::string_name>(&storage_);
  }
  [[nodiscard]] const core::string* try_as_string() const noexcept {
    return std::get_if<core::string>(&storage_);
  }
  [[nodiscard]] const meta::any_ref* try_as_ref() const noexcept {
    return std::get_if<meta::any_ref>(&storage_);
  }
  [[nodiscard]] const meta::any* try_as_any() const noexcept {
    return std::get_if<meta::any>(&storage_);
  }
  [[nodiscard]] meta::any* try_as_any() noexcept { return std::get_if<meta::any>(&storage_); }

  [[nodiscard]] std::optional<std::string_view> try_as_string_view() const noexcept {
    if (const core::string_name* name = try_as_string_name(); name != nullptr) {
      return name->view();
    }
    if (const core::string* text = try_as_string(); text != nullptr) {
      return std::string_view{text->data(), text->size()};
    }
    return std::nullopt;
  }

  [[nodiscard]] meta::any_ref to_ref() noexcept {
    if (meta::any_ref* ref = std::get_if<meta::any_ref>(&storage_); ref != nullptr) {
      return *ref;
    }
    if (meta::any* value = std::get_if<meta::any>(&storage_); value != nullptr) {
      return value->ref();
    }
    if (meta::object* object = as_object(); object != nullptr) {
      return meta::any_ref::from(*object);
    }
    return {};
  }

  [[nodiscard]] meta::any_ref to_ref() const noexcept {
    if (const meta::any_ref* ref = try_as_ref(); ref != nullptr) {
      return *ref;
    }
    if (const meta::any* value = try_as_any(); value != nullptr) {
      return value->ref();
    }
    if (meta::object* object = as_object(); object != nullptr) {
      return meta::any_ref::from_const(*object);
    }
    return {};
  }

  [[nodiscard]] meta::any to_any() const {
    switch (kind()) {
    case value_kind::boolean:
      return meta::any::from(as_bool());
    case value_kind::signed_integer:
      return meta::any::from(as_i64());
    case value_kind::unsigned_integer:
      return meta::any::from(as_u64());
    case value_kind::floating:
      return meta::any::from(as_f64());
    case value_kind::string_name:
      return meta::any::from(as_string_name());
    case value_kind::string:
      return meta::any::from(*try_as_string());
    case value_kind::reference:
      return as_ref().to_any();
    case value_kind::any:
      return *try_as_any();
    default:
      return {};
    }
  }

  /**
   * @brief 转换为反射方法调用参数。
   *
   * `reference` 和 `object` 会作为左值引用参数传递；`any`、标量和文本会复制为拥有的右值参数。
   * `null` 当前无法表达有效 C++ 参数，会返回无效参数。
   */
  [[nodiscard]] meta::argument to_argument() const {
    switch (kind()) {
    case value_kind::reference:
    case value_kind::object:
      return meta::argument::from(to_ref());
    case value_kind::any:
    case value_kind::boolean:
    case value_kind::signed_integer:
    case value_kind::unsigned_integer:
    case value_kind::floating:
    case value_kind::string_name:
    case value_kind::string:
      return meta::argument::move(to_any());
    default:
      return {};
    }
  }

private:
  using storage_type =
      std::variant<std::monostate, bool, std::int64_t, std::uint64_t, double, core::string_name,
                   core::string, meta::object*, meta::any_ref, meta::any>;

  storage_type storage_{};
};

} // namespace caors::meta

#endif
