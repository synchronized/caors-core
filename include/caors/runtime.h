// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RUNTIME_H_
#define CAORS_RUNTIME_H_

#include "caors/export.h"
#include "caors/meta/context.h"
#include "caors/meta/registry.h"

namespace caors {

/** @brief CAORS 运行时创建选项。 */
struct runtime_options {
  /** @brief 当前 runtime 拥有的 meta registry 配置。 */
  meta::registry_options meta_registry{};
};

/**
 * @brief 拥有一组可隔离 CAORS 运行期状态的顶层对象。
 *
 * runtime 当前拥有 meta registry 与对应 context。type/capability identity 和 `string_name`
 * 仍是进程唯一状态，以保证宿主与插件跨模块使用相同标识。
 */
class CAORS_API runtime final {
public:
  runtime();
  explicit runtime(runtime_options options);
  ~runtime();

  runtime(const runtime&) = delete;
  runtime& operator=(const runtime&) = delete;

  /** @brief 返回进程默认 runtime。 */
  [[nodiscard]] static runtime& instance() noexcept;

  /** @brief 返回当前 runtime 拥有的 meta registry。 */
  [[nodiscard]] meta::registry& get_meta_registry() noexcept;
  [[nodiscard]] const meta::registry& get_meta_registry() const noexcept;

  /** @brief 返回定位到当前 registry 的 meta context。 */
  [[nodiscard]] meta::context& get_meta_context() noexcept;
  [[nodiscard]] const meta::context& get_meta_context() const noexcept;

private:
  meta::registry meta_registry_;
  meta::context meta_context_;
};

} // namespace caors

#endif
