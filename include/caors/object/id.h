// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_ID_H_
#define CAORS_OBJECT_ID_H_

#include "caors/core/guid.h"
#include "caors/export.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace caors::objects {

/**
 * @brief 运行期对象实例标识。
 *
 * `0` 保留为无效值。该标识只表达一次进程运行期间的对象身份，不承诺跨进程或跨存档稳定。
 * 普通对象会在构造时通过 `generate_object_id()` 从 CAORS 核心库的全局生成器取得新 id。
 */
class object_id {
public:
  constexpr object_id() noexcept = default;
  explicit constexpr object_id(std::uint64_t value) noexcept : value_(value) {}

  [[nodiscard]] static constexpr object_id invalid() noexcept { return object_id{}; }
  [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return value_ != 0; }

  friend constexpr bool operator==(object_id lhs, object_id rhs) noexcept {
    return lhs.value_ == rhs.value_;
  }

  friend constexpr bool operator!=(object_id lhs, object_id rhs) noexcept { return !(lhs == rhs); }

private:
  std::uint64_t value_ = 0;
};

/** @brief `object_id` 的哈希函数。 */
struct object_id_hash {
  [[nodiscard]] constexpr std::size_t operator()(object_id id) const noexcept {
    const std::uint64_t value = id.value();
    return static_cast<std::size_t>(value ^ (value >> 32U));
  }
};

/**
 * @brief 生成新的运行期对象实例标识。
 *
 * 生成器位于 CAORS 核心库实现文件中，避免不同翻译单元各自持有计数器。正常情况下返回全局递增的
 * 非 0 id；理论上耗尽 `std::uint64_t` 可用空间后返回 `object_id::invalid()`，不会回绕复用旧 id。
 */
[[nodiscard]] CAORS_API object_id generate_object_id() noexcept;

using object_type_id = type_id;
using object_guid = core::guid;
using object_guid_hash = core::guid_hash;

} // namespace caors::objects

namespace std {

template <> struct hash<caors::objects::object_id> {
  [[nodiscard]] constexpr std::size_t operator()(caors::objects::object_id id) const noexcept {
    return caors::objects::object_id_hash{}(id);
  }
};

} // namespace std

namespace caors {

using object_id = objects::object_id;
using object_type_id = objects::object_type_id;
using object_guid = objects::object_guid;
using object_guid_hash = objects::object_guid_hash;
using objects::generate_object_id;

} // namespace caors

#endif
