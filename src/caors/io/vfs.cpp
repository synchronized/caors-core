// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io/vfs.h"
#include "caors/io/file_system_resolver.h"
namespace caors::io {
namespace {
// 旧路由算法仅作为私有传输适配，新的 backend 不继承 file_system。
class backend_transport final : public file_system {
public:
  backend_transport(std::shared_ptr<backend> target, std::shared_ptr<void> lifetime)
      : lifetime_(std::move(lifetime)), target_(std::move(target)) {}
  bool exists(path_view) const override { return false; }
  byte_buffer_result read_all(path_view, memory_resource*) override {
    return {{error_domain::io, errc::unsupported}, {}};
  }
  error write_all(path_view, std::span<const std::byte>) override {
    return {error_domain::io, errc::unsupported};
  }
  io_error start_open(resolved_file location, memory_resource* resource,
                      io_completion<std::shared_ptr<async_file>> completion) override {
    return target_->open(std::move(location.path), {}, resource, std::move(completion));
  }

private:
  std::shared_ptr<void> lifetime_;
  std::shared_ptr<backend> target_;
};
} // namespace
vfs::vfs(memory_resource* resource) : routes_(std::make_shared<file_system_resolver>(resource)) {}
error vfs::mount(uri_view prefix, std::shared_ptr<backend> target, path_view target_prefix,
                 std::shared_ptr<void> lifetime) {
  if (!target)
    return {error_domain::io, errc::invalid_argument};
  return routes_->mount(prefix,
                        std::make_shared<backend_transport>(std::move(target), std::move(lifetime)),
                        target_prefix);
}
bool vfs::unmount(uri_view prefix) { return routes_->unmount(prefix); }
void vfs::clear() { routes_->clear(); }
std::size_t vfs::mount_count() const noexcept { return routes_->mount_count(); }
result<vfs_snapshot> vfs::snapshot() const {
  auto value = route_snapshot::capture(routes_);
  if (!value)
    return {value.error(), {}};
  return {{}, vfs_snapshot{std::move(value.value)}};
}
result<resolved_file> vfs_snapshot::resolve(path_view location) const {
  return routes_.resolve(location);
}
} // namespace caors::io
