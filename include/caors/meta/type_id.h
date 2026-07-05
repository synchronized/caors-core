// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_ID_H_
#define CAORS_META_TYPE_ID_H_

#include "caors/core/type_guid.h"
#include "caors/export.h"

#include <cstdint>
#include <type_traits>
#include <typeinfo>

namespace caors::meta {

/**
 * @brief 运行期类型标识。
 *
 * 对于 C++ 类型，类型标识由库内部 C++ type key 派生，适合当前进程内的注册表索引。它不是类型名
 * hash，也不是跨进程、跨动态库或跨版本序列化 ID。
 */
class type_id {
public:
  constexpr type_id() noexcept = default;
  explicit constexpr type_id(std::uint64_t value) noexcept : value_(value) {}

  [[nodiscard]] static constexpr type_id invalid() noexcept { return type_id{}; }
  [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return value_ != 0; }

  friend constexpr bool operator==(type_id lhs, type_id rhs) noexcept {
    return lhs.value_ == rhs.value_;
  }

  friend constexpr bool operator!=(type_id lhs, type_id rhs) noexcept { return !(lhs == rhs); }

private:
  std::uint64_t value_ = 0;
};

using type_guid = core::type_guid;

namespace detail {

template <typename T> struct cpp_type_id_storage {
  inline static const int key = 0;
};

template <typename T> [[nodiscard]] inline type_id type_id_of_cpp_type() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  static_assert(sizeof(std::uintptr_t) <= sizeof(std::uint64_t));
  return type_id{static_cast<std::uint64_t>(
      reinterpret_cast<std::uintptr_t>(&cpp_type_id_storage<type>::key))};
}

/** @brief 按已注册 C++ RTTI 查询当前进程内类型 id。 */
[[nodiscard]] CAORS_API type_id type_id_of_cpp_type_info(const std::type_info& info) noexcept;

} // namespace detail

} // namespace caors::meta

namespace caors {

using type_id = meta::type_id;

} // namespace caors

#endif
