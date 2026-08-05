// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_PATH_H_
#define CAORS_IO_PATH_H_

#include "caors/core/containers.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace caors::io {

/**
 * @brief 非拥有路径视图。
 *
 * `path_view` 只保存字符串视图，不拥有内容，也不自动解释为宿主文件系统路径。它用于 IO 和资源层的
 * 虚拟路径、资源位置和后端路径参数。
 */
class path_view {
public:
  constexpr path_view() noexcept = default;
  constexpr path_view(std::string_view value) noexcept : value_(value) {}
  path_view(const char* value) noexcept
      : value_(value != nullptr ? std::string_view{value} : std::string_view{}) {}
  path_view(const std::string& value) noexcept : value_(value.data(), value.size()) {}
  path_view(const core::string& value) noexcept : value_(value.data(), value.size()) {}

  /** @brief 返回底层字符串视图。 */
  [[nodiscard]] constexpr std::string_view string_view() const noexcept { return value_; }

  [[nodiscard]] constexpr const char* data() const noexcept { return value_.data(); }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return value_.size(); }
  [[nodiscard]] constexpr bool empty() const noexcept { return value_.empty(); }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return !empty(); }
  [[nodiscard]] constexpr operator std::string_view() const noexcept { return value_; }

private:
  std::string_view value_{};
};

/**
 * @brief 拥有路径字符串的轻量类型。
 *
 * `path` 使用 CAORS allocator 保存内容。默认构造和从字符串构造不会自动规范化；需要虚拟路径规范化时
 * 使用 `normalize_path()` 或 `join_path()`。
 */
class path {
public:
  explicit path(memory_resource* resource = nullptr) : value_{allocator<char>{resource}} {}

  path(path_view value, memory_resource* resource = nullptr) : path(resource) { assign(value); }
  path(const char* value, memory_resource* resource = nullptr) : path(path_view{value}, resource) {}
  path(std::string_view value, memory_resource* resource = nullptr)
      : path(path_view{value}, resource) {}

  /** @brief 替换路径内容。 */
  path& assign(path_view value) {
    const std::string_view text = value.string_view();
    value_.assign(text.begin(), text.end());
    return *this;
  }

  /** @brief 返回内部存储使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept {
    return value_.get_allocator().resource();
  }

  /** @brief 返回非拥有路径视图。 */
  [[nodiscard]] path_view view() const noexcept { return path_view{value_}; }

  /** @brief 返回底层字符串视图。 */
  [[nodiscard]] std::string_view string_view() const noexcept { return view().string_view(); }

  [[nodiscard]] const char* c_str() const noexcept { return value_.c_str(); }
  [[nodiscard]] bool empty() const noexcept { return value_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return value_.size(); }
  [[nodiscard]] explicit operator bool() const noexcept { return !empty(); }
  [[nodiscard]] operator path_view() const noexcept { return view(); }

  /** @brief 返回可拷贝的 CAORS 字符串副本。 */
  [[nodiscard]] core::string to_string(memory_resource* result_resource = nullptr) const {
    core::string result{allocator<char>{result_resource}};
    result.assign(value_.begin(), value_.end());
    return result;
  }

  friend bool operator==(const path& lhs, path_view rhs) noexcept {
    return lhs.string_view() == rhs.string_view();
  }

  friend bool operator==(path_view lhs, const path& rhs) noexcept { return rhs == lhs; }

private:
  core::string value_;
};

/** @brief 规范化虚拟路径：使用 `/`，去掉开头分隔符，并折叠连续分隔符。 */
[[nodiscard]] inline path normalize_path(path_view value,
                                         memory_resource* result_resource = nullptr) {
  path result{result_resource};
  std::string_view text = value.string_view();

  std::size_t index = 0;
  while (index < text.size() && (text[index] == '/' || text[index] == '\\')) {
    ++index;
  }

  bool previous_separator = false;
  core::string normalized{allocator<char>{result_resource}};
  normalized.reserve(text.size());
  for (; index < text.size(); ++index) {
    const char current = text[index];
    const bool separator = current == '/' || current == '\\';
    if (separator) {
      if (!previous_separator) {
        normalized.push_back('/');
      }
      previous_separator = true;
      continue;
    }

    normalized.push_back(current);
    previous_separator = false;
  }

  while (!normalized.empty() && normalized.back() == '/') {
    normalized.pop_back();
  }

  result.assign(path_view{normalized});
  return result;
}

/** @brief 拼接并规范化两个虚拟路径片段。 */
[[nodiscard]] inline path join_path(path_view base, path_view suffix,
                                    memory_resource* result_resource = nullptr) {
  path left = normalize_path(base, result_resource);
  path right = normalize_path(suffix, result_resource);

  if (!left) {
    return right;
  }
  if (!right) {
    return left;
  }

  core::string joined{allocator<char>{result_resource}};
  const std::string_view left_view = left.string_view();
  const std::string_view right_view = right.string_view();
  joined.reserve(left_view.size() + 1 + right_view.size());
  joined.append(left_view.begin(), left_view.end());
  joined.push_back('/');
  joined.append(right_view.begin(), right_view.end());
  return path{path_view{joined}, result_resource};
}

} // namespace caors::io

#endif
