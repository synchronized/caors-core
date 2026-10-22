// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ASSETS_PROJECT_BUILDER_H_
#define CAORS_ASSETS_PROJECT_BUILDER_H_

#include "caors/assets/asset_manager.h"

namespace caors::assets {
/**
 * @brief 按工程配置装配资源环境；不参与日常资源请求。
 * @note 借用 manager，必须在其所属线程调用且 manager 须覆盖本对象寿命。
 * 配置创建的对象暂由 manager 持有，builder 析构不清理它们；实际操作未结束时构建返回 frozen。
 * 构建失败可能留下部分配置，沿用既有逐阶段构建语义，不提供事务回滚。
 */
class CAORS_API project_builder final {
public:
  /** @brief 绑定待装配的资源环境，不转移所有权。 */
  explicit project_builder(asset_manager& manager) noexcept : manager_(manager) {}
  /** @brief 按配置重建环境，返回首个错误；手工绑定按既有规则保留。 */
  [[nodiscard]] error build(const project_config& config);
  /** @brief 执行相同装配流程，并返回失败阶段及诊断。 */
  [[nodiscard]] asset_build_result build_detailed(const project_config& config);

private:
  asset_manager& manager_;
};
} // namespace caors::assets

#endif
