// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_MANIFEST_BINDING_H_
#define CAORS_RESOURCES_MANIFEST_BINDING_H_

#include "caors/resources/manifest.h"
#include "caors/resources/source.h"

namespace caors::resources {
/**
 * @brief 把清单中的 GUID/location 批量绑定到 provider。
 *
 * `bind_manifest()` 不拥有 source，也不回滚已经成功绑定的条目；调用方需要在失败时按自身策略处理
 * provider 中已添加的绑定。
 */
[[nodiscard]] inline error bind_manifest(provider& target, source& source,
                                         const manifest& entries) {
  error status{};
  entries.for_each([&](const manifest_entry& entry) {
    if (status.failed()) {
      return;
    }

    status = target.bind(entry.guid, source, entry.location);
  });

  return status;
}

/**
 * @brief 把清单中的 GUID/location 批量同步到 provider。
 *
 * `sync_manifest()` 会对每个条目调用 `provider::rebind()`：已存在的 GUID 会更新位置或 source，
 * 不存在的 GUID 会新增。它不会删除 provider 中清单未包含的旧绑定。
 */
[[nodiscard]] inline error sync_manifest(provider& target, source& source,
                                         const manifest& entries) {
  error status{};
  entries.for_each([&](const manifest_entry& entry) {
    if (status.failed()) {
      return;
    }

    status = target.rebind(entry.guid, source, entry.location);
  });

  return status;
}

/**
 * @brief 使用完整清单替换 provider 中的全部 GUID/location 绑定。
 *
 * `replace_manifest()` 会先清空 provider，再把清单条目重新绑定进去。它适合资源数据库或调试
 * fileserver 下发完整清单的场景；如果绑定失败，函数不会恢复清空前的 provider 状态。
 */
[[nodiscard]] inline error replace_manifest(provider& target, source& source,
                                            const manifest& entries) {
  target.clear();
  return bind_manifest(target, source, entries);
}

} // namespace caors::resources

#endif
