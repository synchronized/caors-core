// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_ID_H_
#define CAORS_META_TYPE_ID_H_

#include "caors/core/type_guid.h"
#include "caors/export.h"
#include "caors/meta/detail/canonical_type_name.h"
#include "caors/meta/identity_id.h"

#include <cstddef>
#include <cstdint>
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

/**
 * @brief 按 canonical C++ 类型名称获取进程内唯一类型 ID。
 *
 * 名称文本只在调用期间借用，runtime 会为首次出现的名称保存副本。名称不能为空。首次创建
 * 所需的内存分配失败或 ID 空间耗尽属于不可恢复的 runtime 初始化失败。使用指针和长度参数
 * 是为了避免在动态库 ABI 上传递标准库字符串类型。
 */
[[nodiscard]] CAORS_API type_id get_or_create_type_id(const char* canonical_name,
                                                      std::size_t length) noexcept;

template <typename T> [[nodiscard]] inline type_id type_id_of_cpp_type() noexcept {
  using type = std::remove_cvref_t<T>;
  constexpr std::string_view canonical_name = canonical_cpp_type_name<type>();
  static const type_id id = get_or_create_type_id(canonical_name.data(), canonical_name.size());
  return id;
}

/** @brief 按已注册 C++ RTTI 查询当前进程内类型 id。 */
[[nodiscard]] CAORS_API type_id type_id_of_cpp_type_info(const std::type_info& info) noexcept;

} // namespace detail

} // namespace caors::meta

namespace caors {

using type_id = meta::type_id;

} // namespace caors

#endif
