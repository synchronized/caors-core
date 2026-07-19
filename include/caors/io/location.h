// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_LOCATION_H_
#define CAORS_IO_LOCATION_H_

#include "caors/io/path.h"
#include "caors/io/uri.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace caors::io {

/**
 * @brief 非拥有资源位置视图。
 *
 * `location_view` 表达资源层可读写位置，可以是普通虚拟路径，也可以是带 scheme 的 URI。
 * 它只保存字符串视图，不拥有内容，也不承诺底层一定来自某个具体文件系统。
 */
class location_view {
public:
  constexpr location_view() noexcept = default;
  constexpr location_view(std::string_view value) noexcept : value_(value) {}
  location_view(const char* value) noexcept
      : value_(value != nullptr ? std::string_view{value} : std::string_view{}) {}
  location_view(const std::string& value) noexcept : value_(value.data(), value.size()) {}
  location_view(const core::string& value) noexcept : value_(value.data(), value.size()) {}
  location_view(path_view value) noexcept : value_(value.string_view()) {}
  location_view(uri_view value) noexcept : value_(value.string_view()) {}

  /** @brief 返回底层字符串视图。 */
  [[nodiscard]] constexpr std::string_view string_view() const noexcept { return value_; }

  [[nodiscard]] constexpr const char* data() const noexcept { return value_.data(); }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return value_.size(); }
  [[nodiscard]] constexpr bool empty() const noexcept { return value_.empty(); }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return !empty(); }
  [[nodiscard]] constexpr operator std::string_view() const noexcept { return value_; }
  [[nodiscard]] constexpr operator path_view() const noexcept { return path_view{value_}; }

private:
  std::string_view value_{};
};

/**
 * @brief 拥有资源位置字符串的轻量类型。
 *
 * `location` 使用 CAORS allocator 保存内容。默认构造和从字符串构造不会自动规范化；
 * 需要统一路径分隔符和 URI path 时使用 `normalize_location()` 或 `join_location()`。
 */
class location {
public:
  explicit location(memory_resource* resource = nullptr) : value_{allocator<char>{resource}} {}

  location(location_view value, memory_resource* resource = nullptr) : location(resource) {
    assign(value);
  }
  location(path_view value, memory_resource* resource = nullptr)
      : location(location_view{value}, resource) {}
  location(uri_view value, memory_resource* resource = nullptr)
      : location(location_view{value}, resource) {}
  location(const char* value, memory_resource* resource = nullptr)
      : location(location_view{value}, resource) {}
  location(std::string_view value, memory_resource* resource = nullptr)
      : location(location_view{value}, resource) {}

  /** @brief 替换资源位置内容。 */
  location& assign(location_view value) {
    const std::string_view text = value.string_view();
    value_.assign(text.begin(), text.end());
    return *this;
  }

  /** @brief 返回内部存储使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept {
    return value_.get_allocator().resource();
  }

  /** @brief 返回非拥有资源位置视图。 */
  [[nodiscard]] location_view view() const noexcept { return location_view{value_}; }

  /** @brief 返回底层字符串视图。 */
  [[nodiscard]] std::string_view string_view() const noexcept { return view().string_view(); }

  [[nodiscard]] const char* c_str() const noexcept { return value_.c_str(); }
  [[nodiscard]] bool empty() const noexcept { return value_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return value_.size(); }
  [[nodiscard]] explicit operator bool() const noexcept { return !empty(); }
  [[nodiscard]] operator location_view() const noexcept { return view(); }
  [[nodiscard]] operator path_view() const noexcept { return path_view{value_}; }

  /** @brief 返回可拷贝的 CAORS 字符串副本。 */
  [[nodiscard]] core::string to_string(memory_resource* resource = nullptr) const {
    core::string result{allocator<char>{resource}};
    result.assign(value_.begin(), value_.end());
    return result;
  }

  friend bool operator==(const location& lhs, location_view rhs) noexcept {
    return lhs.string_view() == rhs.string_view();
  }

  friend bool operator==(location_view lhs, const location& rhs) noexcept { return rhs == lhs; }

private:
  core::string value_;
};

namespace detail {

[[nodiscard]] inline bool uses_authority_form(uri_view value) noexcept {
  const std::string_view text = value.string_view();
  const std::string_view scheme = value.scheme();
  return !scheme.empty() && text.size() >= scheme.size() + 3 &&
         text.substr(scheme.size() + 1).starts_with("//");
}

[[nodiscard]] inline location make_location_from_parts(std::string_view scheme, bool authority_form,
                                                       std::string_view authority,
                                                       path_view value_path,
                                                       memory_resource* resource) {
  if (scheme.empty()) {
    return location{normalize_path(value_path, resource), resource};
  }

  path normalized_path = normalize_path(value_path, resource);
  const std::string_view path_text = normalized_path.string_view();
  core::string result{allocator<char>{resource}};

  if (authority_form) {
    result.reserve(scheme.size() + 3 + authority.size() + path_text.size() +
                   (!path_text.empty() ? 1U : 0U));
    result.append(scheme.begin(), scheme.end());
    result.append("://");
    result.append(authority.begin(), authority.end());
    if (!path_text.empty()) {
      result.push_back('/');
      result.append(path_text.begin(), path_text.end());
    }
  } else {
    result.reserve(scheme.size() + 1 + path_text.size());
    result.append(scheme.begin(), scheme.end());
    result.push_back(':');
    result.append(path_text.begin(), path_text.end());
  }

  return location{location_view{result}, resource};
}

} // namespace detail

/**
 * @brief 规范化资源位置。
 *
 * 普通路径按虚拟路径规则规范化；带 scheme 的 URI 会保留 scheme / authority，只规范化 path 部分。
 */
[[nodiscard]] inline location normalize_location(location_view value,
                                                 memory_resource* resource = nullptr) {
  const uri_view parsed{value.string_view()};
  return detail::make_location_from_parts(parsed.scheme(), detail::uses_authority_form(parsed),
                                          parsed.authority(), path_view{parsed.path()}, resource);
}

/**
 * @brief 拼接并规范化两个资源位置片段。
 *
 * `suffix` 如果本身是 URI，则直接作为完整位置规范化返回；否则拼到 `base` 的 path 部分。
 */
[[nodiscard]] inline location join_location(location_view base, location_view suffix,
                                            memory_resource* resource = nullptr) {
  const uri_view suffix_uri{suffix.string_view()};
  if (suffix_uri.has_scheme()) {
    return normalize_location(suffix, resource);
  }

  const location normalized_base = normalize_location(base, resource);
  const uri_view parsed_base{normalized_base.string_view()};
  if (!parsed_base.has_scheme()) {
    return location{join_path(normalized_base, suffix, resource), resource};
  }

  const path joined_path = join_path(path_view{parsed_base.path()}, suffix, resource);
  return detail::make_location_from_parts(parsed_base.scheme(),
                                          detail::uses_authority_form(parsed_base),
                                          parsed_base.authority(), joined_path, resource);
}

} // namespace caors::io

#endif
