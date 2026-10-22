// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_LOAD_SNAPSHOT_H_
#define CAORS_RESOURCES_LOAD_SNAPSHOT_H_

#include <functional>
#include <memory>

namespace caors::resources {
/**
 * @brief 加载批次持有的不可变来源数据基类，不规定文件系统或地址协议。
 * @note 具体适配负责捕获一致视图；共享所有权仅保证保活，不自动冻结外部可变数据。
 * 派生对象及其宿主凭据须覆盖整个批次，模块卸载前须清理全部使用者。
 */
class load_snapshot {
public:
  virtual ~load_snapshot() = default;
};
/**
 * @brief 在发起线程捕获批次数据；空结果表示不携带数据，不触发其他来源回退。
 * @note 捕获器须在调用期间有效，不得重入或修改加载配置；抛出的异常交由发起方处理。
 */
using snapshot_capture = std::function<std::shared_ptr<const load_snapshot>()>;
} // namespace caors::resources

#endif
