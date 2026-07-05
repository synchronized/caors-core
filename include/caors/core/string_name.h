// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_BASE_STRING_NAME_H_
#define CAORS_BASE_STRING_NAME_H_

#include "caors/core/containers.h"
#include "caors/export.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace caors::core {

namespace detail {
struct string_name_entry;
}

/**
 * @brief 运行期字符串名。
 *
 * `string_name` 会把文本加入进程内字符串表，相同文本共享同一个稳定 entry 指针。该指针只用于
 * 本次进程运行期的快速比较和查找，不承诺跨进程或跨版本稳定。
 */
class CAORS_API string_name {
public:
  constexpr string_name() noexcept = default;
  explicit string_name(const char* name);
  explicit string_name(std::string_view name);
  explicit string_name(const std::string& name);

  [[nodiscard]] static constexpr string_name invalid() noexcept { return string_name{}; }
  [[nodiscard]] static std::optional<string_name> find(std::string_view name);

  [[nodiscard]] constexpr bool empty() const noexcept { return entry_ == nullptr; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return entry_ != nullptr; }
  [[nodiscard]] std::string_view view() const noexcept;
  [[nodiscard]] caors::core::string string(memory::memory_resource* resource = nullptr) const;
  [[nodiscard]] std::string to_string() const;

  friend constexpr bool operator==(string_name lhs, string_name rhs) noexcept {
    return lhs.entry_ == rhs.entry_;
  }

  friend constexpr bool operator!=(string_name lhs, string_name rhs) noexcept {
    return !(lhs == rhs);
  }

private:
  friend struct string_name_hash;

  explicit constexpr string_name(const detail::string_name_entry* entry) noexcept : entry_(entry) {}

  const detail::string_name_entry* entry_ = nullptr;
};

/** @brief `string_name` 的运行期哈希函数。 */
struct string_name_hash {
  [[nodiscard]] std::size_t operator()(string_name name) const noexcept {
    return std::hash<const detail::string_name_entry*>{}(name.entry_);
  }
};

[[nodiscard]] CAORS_API bool operator==(string_name lhs, std::string_view rhs);
[[nodiscard]] CAORS_API bool operator==(std::string_view lhs, string_name rhs);
[[nodiscard]] CAORS_API bool operator!=(string_name lhs, std::string_view rhs);
[[nodiscard]] CAORS_API bool operator!=(std::string_view lhs, string_name rhs);

} // namespace caors::core

namespace caors {

using core::operator!=;
using core::operator==;
using core::string_name;
using core::string_name_hash;

} // namespace caors

namespace std {

template <> struct hash<caors::core::string_name> {
  [[nodiscard]] std::size_t operator()(caors::core::string_name name) const noexcept {
    return caors::core::string_name_hash{}(name);
  }
};

} // namespace std

#endif
