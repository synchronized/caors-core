// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_ID_H_
#define CAORS_OBJECT_ID_H_

#include "caors/export.h"
#include "caors/meta/type_id.h"

#include <cstdint>

namespace caors::objects {

/**
 * @brief 运行期对象实例标识。
 *
 * `0` 保留为无效值。该标识只表达一次进程运行期间的对象身份，不承诺跨进程或跨存档稳定。
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

/** @brief 生成新的运行期对象实例标识。 */
[[nodiscard]] CAORS_API object_id generate_object_id() noexcept;

using object_type_id = type_id;

} // namespace caors::objects

namespace caors {

using object_id = objects::object_id;
using object_type_id = objects::object_type_id;
using objects::generate_object_id;

} // namespace caors

#endif
