// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_BASE_STRING_NAME_H_
#define CAORS_BASE_STRING_NAME_H_

#include "caors/export.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace caors {

/**
 * @brief 运行期字符串名。
 *
 * `string_name` 会把文本加入进程内字符串表，相同文本共享同一个运行期 id。该 id 只用于本次
 * 进程运行期的快速比较和查找，不承诺跨进程或跨版本稳定。
 */
class CAORS_API string_name {
public:
  constexpr string_name() noexcept = default;
  explicit string_name(const char* name);
  explicit string_name(std::string_view name);
  explicit string_name(const std::string& name);

  [[nodiscard]] static constexpr string_name invalid() noexcept { return string_name{}; }
  [[nodiscard]] static std::optional<string_name> find(std::string_view name);

  [[nodiscard]] constexpr std::uint64_t id() const noexcept { return id_; }
  [[nodiscard]] constexpr bool empty() const noexcept { return id_ == 0; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return id_ != 0; }
  [[nodiscard]] std::string_view view() const noexcept;
  [[nodiscard]] std::string string() const;

  friend constexpr bool operator==(string_name lhs, string_name rhs) noexcept {
    return lhs.id_ == rhs.id_;
  }

  friend constexpr bool operator!=(string_name lhs, string_name rhs) noexcept {
    return !(lhs == rhs);
  }

private:
  explicit constexpr string_name(std::uint64_t id) noexcept : id_(id) {}

  std::uint64_t id_ = 0;
};

[[nodiscard]] CAORS_API bool operator==(string_name lhs, std::string_view rhs);
[[nodiscard]] CAORS_API bool operator==(std::string_view lhs, string_name rhs);
[[nodiscard]] CAORS_API bool operator!=(string_name lhs, std::string_view rhs);
[[nodiscard]] CAORS_API bool operator!=(std::string_view lhs, string_name rhs);

}

#endif
