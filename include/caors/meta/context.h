// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CONTEXT_H_
#define CAORS_META_CONTEXT_H_

#include "caors/export.h"
#include "caors/meta/registry.h"

namespace caors::meta {

/**
 * @brief 元数据 API 的注册表定位上下文。
 *
 * 普通代码使用 `default_context()`；需要隔离注册表或插件注册阶段时，可以显式传递
 * `context`，让 meta / serialization 等模块从同一个上下文定位 registry。
 */
class CAORS_API context final {
public:
  explicit context(registry& registry) noexcept;

  [[nodiscard]] registry& get_registry() noexcept;
  [[nodiscard]] const registry& get_registry() const noexcept;

private:
  registry* registry_ = nullptr;
};

/** @brief 进程内默认元数据上下文。 */
[[nodiscard]] CAORS_API context& default_context() noexcept;

} // namespace caors::meta

#endif
