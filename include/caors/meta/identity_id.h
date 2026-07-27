// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_IDENTITY_ID_H_
#define CAORS_META_IDENTITY_ID_H_

#include "caors/export.h"

#include <cstddef>
#include <cstdint>

namespace caors::meta {

/** @brief capability concept 的进程内查询 ID。 */
class capability_id {
public:
  constexpr capability_id() noexcept = default;
  explicit constexpr capability_id(std::uint64_t value) noexcept : value_(value) {}

  [[nodiscard]] static constexpr capability_id invalid() noexcept { return capability_id{}; }
  [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return value_ != 0; }

  friend constexpr bool operator==(capability_id lhs, capability_id rhs) noexcept {
    return lhs.value_ == rhs.value_;
  }

  friend constexpr bool operator!=(capability_id lhs, capability_id rhs) noexcept {
    return !(lhs == rhs);
  }

private:
  std::uint64_t value_ = 0;
};

/** @brief registry extension slot 的进程内查询 ID。 */
class extension_id {
public:
  constexpr extension_id() noexcept = default;
  explicit constexpr extension_id(std::uint64_t value) noexcept : value_(value) {}

  [[nodiscard]] static constexpr extension_id invalid() noexcept { return extension_id{}; }
  [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return value_ != 0; }

  friend constexpr bool operator==(extension_id lhs, extension_id rhs) noexcept {
    return lhs.value_ == rhs.value_;
  }

  friend constexpr bool operator!=(extension_id lhs, extension_id rhs) noexcept {
    return !(lhs == rhs);
  }

private:
  std::uint64_t value_ = 0;
};

using capability_key = capability_id;
using extension_key = extension_id;

namespace detail {

using ::caors::meta::capability_id;
using ::caors::meta::capability_key;
using ::caors::meta::extension_id;
using ::caors::meta::extension_key;

/** @brief identity 发号器的内部 domain。 */
enum class identity_domain {
  type,
  capability,
  extension,
};

/**
 * @brief 按 domain 和 canonical name 获取进程内 ID。
 *
 * 该函数只负责分配当前 runtime 内的数字身份，不保存 registry 数据，也不提供跨进程稳定性。
 */
[[nodiscard]] CAORS_API std::uint64_t get_or_create_identity_id(identity_domain domain,
                                                                const char* canonical_name,
                                                                std::size_t length) noexcept;

} // namespace detail

} // namespace caors::meta

#endif
