// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_PLUGINS_PLUGIN_LIBRARY_LEASE_H_
#define CAORS_PLUGINS_PLUGIN_LIBRARY_LEASE_H_

#include "caors/export.h"

#include <cstddef>
#include <filesystem>
#include <memory>

namespace caors::plugins {

class plugin_binary;

/**
 * @brief 保持插件动态库处于加载状态的共享租约。
 *
 * 租约不允许直接访问平台句柄。只要任意租约仍存在，底层动态库就不会执行
 * `FreeLibrary` 或 `dlclose`。
 */
class CAORS_API plugin_library_lease final {
public:
  plugin_library_lease() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] const std::filesystem::path& path() const noexcept;
  [[nodiscard]] std::size_t use_count() const noexcept;

private:
  friend class plugin_binary;

  struct impl;
  explicit plugin_library_lease(std::shared_ptr<impl> data) noexcept : data_(std::move(data)) {}

  std::shared_ptr<impl> data_{};
};

} // namespace caors::plugins

#endif
