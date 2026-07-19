// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_URI_H_
#define CAORS_IO_URI_H_

#include "caors/io/path.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace caors::io {

/**
 * @brief 非拥有 URI 视图。
 *
 * `uri_view` 用于在 IO 和资源层表达逻辑位置，例如 `caors://assets/texture.png`、
 * `file://project/config.json` 或普通虚拟路径 `assets/texture.png`。第一版只解析 scheme、
 * authority 和 path，不处理百分号解码、查询参数或片段语义。
 */
class uri_view {
public:
  uri_view() noexcept = default;
  uri_view(std::string_view value) noexcept : value_(value) { parse(); }
  uri_view(const char* value) noexcept
      : uri_view(value != nullptr ? std::string_view{value} : std::string_view{}) {}
  uri_view(const std::string& value) noexcept : uri_view(std::string_view{value}) {}
  uri_view(const core::string& value) noexcept : uri_view(std::string_view{value}) {}
  uri_view(path_view value) noexcept : uri_view(value.string_view()) {}

  /** @brief 返回完整 URI 文本。 */
  [[nodiscard]] std::string_view string_view() const noexcept { return value_; }

  /** @brief 返回 URI scheme；普通路径没有 scheme。 */
  [[nodiscard]] std::string_view scheme() const noexcept { return scheme_; }

  /** @brief 返回 `//` 后面的 authority；普通路径或 `scheme:path` 没有 authority。 */
  [[nodiscard]] std::string_view authority() const noexcept { return authority_; }

  /** @brief 返回用于传递给后端文件系统的路径部分。 */
  [[nodiscard]] std::string_view path() const noexcept { return path_; }

  [[nodiscard]] bool empty() const noexcept { return value_.empty(); }
  [[nodiscard]] bool has_scheme() const noexcept { return !scheme_.empty(); }
  [[nodiscard]] bool has_authority() const noexcept { return !authority_.empty(); }
  [[nodiscard]] explicit operator bool() const noexcept { return !empty(); }

private:
  void parse() noexcept;

  std::string_view value_{};
  std::string_view scheme_{};
  std::string_view authority_{};
  std::string_view path_{};
};

/**
 * @brief 拥有 URI 字符串的轻量类型。
 *
 * `uri` 使用 CAORS allocator 保存文本，解析规则与 `uri_view` 一致。
 */
class uri {
public:
  explicit uri(memory_resource* resource = nullptr) : value_{allocator<char>{resource}} {}

  uri(uri_view value, memory_resource* resource = nullptr) : uri(resource) { assign(value); }
  uri(const char* value, memory_resource* resource = nullptr) : uri(uri_view{value}, resource) {}
  uri(std::string_view value, memory_resource* resource = nullptr)
      : uri(uri_view{value}, resource) {}

  /** @brief 替换 URI 文本。 */
  uri& assign(uri_view value) {
    const std::string_view text = value.string_view();
    value_.assign(text.begin(), text.end());
    return *this;
  }

  /** @brief 返回内部存储使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept {
    return value_.get_allocator().resource();
  }

  /** @brief 返回非拥有 URI 视图。 */
  [[nodiscard]] uri_view view() const noexcept { return uri_view{value_}; }

  /** @brief 返回完整 URI 文本。 */
  [[nodiscard]] std::string_view string_view() const noexcept { return view().string_view(); }

  /** @brief 返回 URI scheme。 */
  [[nodiscard]] std::string_view scheme() const noexcept { return view().scheme(); }

  /** @brief 返回 URI authority。 */
  [[nodiscard]] std::string_view authority() const noexcept { return view().authority(); }

  /** @brief 返回 URI path。 */
  [[nodiscard]] std::string_view path() const noexcept { return view().path(); }

  [[nodiscard]] bool empty() const noexcept { return value_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return value_.size(); }
  [[nodiscard]] explicit operator bool() const noexcept { return !empty(); }
  [[nodiscard]] operator uri_view() const noexcept { return view(); }

  /** @brief 返回可拷贝的 CAORS 字符串副本。 */
  [[nodiscard]] core::string to_string(memory_resource* resource = nullptr) const {
    core::string result{allocator<char>{resource}};
    result.assign(value_.begin(), value_.end());
    return result;
  }

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

[[nodiscard]] inline path make_location_from_parts(std::string_view scheme, bool authority_form,
                                                   std::string_view authority, path_view value_path,
                                                   memory_resource* resource) {
  if (scheme.empty()) {
    return normalize_path(value_path, resource);
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

  return path{path_view{result}, resource};
}

} // namespace detail

/**
 * @brief 规范化资源位置。
 *
 * 普通路径按虚拟路径规则规范化；带 scheme 的 URI 会保留 scheme / authority，只规范化 path 部分。
 */
[[nodiscard]] inline path normalize_location(path_view value, memory_resource* resource = nullptr) {
  const uri_view parsed{value};
  return detail::make_location_from_parts(parsed.scheme(), detail::uses_authority_form(parsed),
                                          parsed.authority(), path_view{parsed.path()}, resource);
}

/**
 * @brief 拼接并规范化两个资源位置片段。
 *
 * `suffix` 如果本身是 URI，则直接作为完整位置规范化返回；否则拼到 `base` 的 path 部分。
 */
[[nodiscard]] inline path join_location(path_view base, path_view suffix,
                                        memory_resource* resource = nullptr) {
  const uri_view suffix_uri{suffix};
  if (suffix_uri.has_scheme()) {
    return normalize_location(suffix, resource);
  }

  const path normalized_base = normalize_location(base, resource);
  const uri_view parsed_base{normalized_base};
  if (!parsed_base.has_scheme()) {
    return join_path(normalized_base, suffix, resource);
  }

  const path joined_path = join_path(path_view{parsed_base.path()}, suffix, resource);
  return detail::make_location_from_parts(parsed_base.scheme(),
                                          detail::uses_authority_form(parsed_base),
                                          parsed_base.authority(), joined_path, resource);
}

} // namespace caors::io

#endif
