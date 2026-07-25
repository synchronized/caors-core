// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_ID_H_
#define CAORS_META_TYPE_ID_H_

#include "caors/core/type_guid.h"
#include "caors/export.h"
#include "caors/meta/detail/canonical_type_name.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>

namespace caors::meta {

/**
 * @brief 运行期类型标识。
 *
 * 对于 C++ 类型，类型标识由共享 CAORS runtime 按 canonical C++ 类型名称分配，适合当前进程
 * 内的注册表索引。它不是类型名 hash，也不是跨进程或跨版本序列化 ID。
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

/** @brief CAORS runtime 持有的进程内 C++ 类型身份节点。 */
struct type_identity final {
  type_id id{};
  std::string canonical_name{};
};

/**
 * @brief 按 canonical C++ 类型名称获取进程内唯一身份节点。
 *
 * 返回的节点由 CAORS runtime 持有，在 runtime 生命周期内地址稳定。名称不能为空。首次创建
 * 所需的内存分配失败或 ID 空间耗尽属于不可恢复的 runtime 初始化失败。
 */
[[nodiscard]] CAORS_API const type_identity&
get_or_create_type_identity(std::string_view canonical_name) noexcept;

/** @brief 返回当前编译模块缓存的 C++ 类型身份节点。 */
template <typename T> [[nodiscard]] inline const type_identity* static_type_identity() noexcept {
  using type = std::remove_cvref_t<T>;
  static const type_identity* identity =
      &get_or_create_type_identity(canonical_cpp_type_name<type>());
  return identity;
}

template <typename T> [[nodiscard]] inline type_id type_id_of_cpp_type() noexcept {
  return static_type_identity<T>()->id;
}

/** @brief 按已注册 C++ RTTI 查询当前进程内类型 id。 */
[[nodiscard]] CAORS_API type_id type_id_of_cpp_type_info(const std::type_info& info) noexcept;

} // namespace detail

} // namespace caors::meta

namespace caors {

using type_id = meta::type_id;

} // namespace caors

#endif
