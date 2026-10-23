// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_RESOURCES_VFS_SOURCE_H_
#define CAORS_RESOURCES_VFS_SOURCE_H_
#include "caors/io/vfs.h"
#include "caors/resources/source.h"
namespace caors::resources {
/**
 * @brief 将新 VFS 的批次快照接入资源定位，不在 loader 内读取实时路由。
 * @note 借用 vfs 至快照捕获；已发请求拥有实际路由和后端。仅支持异步快照访问，
 * 旧 source 同步 read/write 不适用，exists 返回 false，不能用于同步 saver。
 */
class CAORS_API vfs_source final : public source {
public:
  explicit vfs_source(const io::vfs& routes) : routes_(&routes) {}
  vfs_source(io::vfs&&) = delete;
  [[nodiscard]] result<captured_source> capture() const override;
  [[nodiscard]] bool exists(io::location_view) const override { return false; }
  [[nodiscard]] io::byte_buffer_result read(io::location_view,
                                            memory_resource* = nullptr) override {
    return {{error_domain::resources, errc::unsupported}, {}};
  }

private:
  const io::vfs* routes_;
};
} // namespace caors::resources
#endif
