// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_RESOURCE_H_
#define CAORS_RESOURCES_RESOURCE_H_

#include "caors/export.h"
#include "caors/resources/id.h"

namespace caors::resources {

/**
 * @brief 资源基类。
 *
 * 资源表示可持久化、可缓存、可复用的资产。该基类只保存稳定资源 GUID，不负责加载、缓存、
 * 引用计数或线程同步；这些策略由后续 `resources::manager` 或上层引擎集成提供。
 */
class CAORS_API resource {
public:
  resource() noexcept = default;
  explicit resource(resource_guid guid) noexcept : guid_(guid) {}
  virtual ~resource() = default;

  resource(const resource&) = delete;
  resource& operator=(const resource&) = delete;
  resource(resource&&) = delete;
  resource& operator=(resource&&) = delete;

  /** @brief 返回资源稳定 GUID。nil 表示尚未绑定持久身份。 */
  [[nodiscard]] resource_guid guid() const noexcept { return guid_; }

  /** @brief 设置资源稳定 GUID。调用方需要保证不会破坏外部索引一致性。 */
  void set_guid(resource_guid guid) noexcept { guid_ = guid; }

  /** @brief 当前资源是否已经绑定非 nil GUID。 */
  [[nodiscard]] bool has_guid() const noexcept { return static_cast<bool>(guid_); }

private:
  resource_guid guid_{};
};

} // namespace caors::resources

namespace caors {

using resource = resources::resource;

} // namespace caors

#endif
