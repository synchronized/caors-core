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
class CAORS_API uri_view {
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

} // namespace caors::io

#endif
