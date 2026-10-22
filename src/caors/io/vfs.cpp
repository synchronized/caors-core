// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io/vfs.h"
#include "caors/io/detail/vfs_operations.h"
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
  backend& target() const noexcept { return *target_; }
  io_error start_open_options(resolved_file location, open_options options,
                              memory_resource* resource,
                              io_completion<std::shared_ptr<async_file>> completion) override {
    return target_->open(std::move(location.path), options, resource, std::move(completion));
  }
  io_error start_open(resolved_file location, memory_resource* resource,
                      io_completion<std::shared_ptr<async_file>> completion) override {
    return target_->open(std::move(location.path), {}, resource, std::move(completion));
  }

private:
  std::shared_ptr<void> lifetime_;
  std::shared_ptr<backend> target_;
};
// 将已冻结的新 VFS 快照交给旧资源来源捕获器，兼容面不提供同步读写回退。
class snapshot_transport final : public file_system {
public:
  explicit snapshot_transport(route_snapshot snapshot) : snapshot_(std::move(snapshot)) {}
  bool exists(path_view) const override { return false; }
  byte_buffer_result read_all(path_view, memory_resource*) override {
    return {{error_domain::io, errc::unsupported}, {}};
  }
  error write_all(path_view, std::span<const std::byte>) override {
    return {error_domain::io, errc::unsupported};
  }
  bool routes_paths() const noexcept override { return true; }
  result<resolved_file> resolve_owned(path_view location) const override {
    return snapshot_.resolve(location);
  }
  result<std::shared_ptr<file_system>> freeze_routes(detail::snapshot_context&,
                                                     std::shared_ptr<void> owner) const override {
    if (!owner)
      return {{error_domain::io, errc::unsupported}, {}};
    return {{},
            std::shared_ptr<file_system>{std::move(owner), const_cast<snapshot_transport*>(this)}};
  }

private:
  route_snapshot snapshot_;
};
} // namespace
namespace detail {
std::shared_ptr<file_system> legacy_snapshot(vfs_snapshot snapshot) {
  return std::make_shared<snapshot_transport>(std::move(snapshot.routes_));
}

io_error start_write(resolved_file source, byte_buffer bytes,
                     io_completion<std::uint64_t> completion) {
  auto* transport = dynamic_cast<backend_transport*>(source.backend.get());
  return transport ? transport->target().write(std::move(source.path), std::move(bytes),
                                               std::move(completion))
                   : io_error{io_errc::unsupported};
}
io_error start_stat(resolved_file source, io_completion<file_status> completion) {
  auto* transport = dynamic_cast<backend_transport*>(source.backend.get());
  return transport ? transport->target().stat(std::move(source.path), std::move(completion))
                   : io_error{io_errc::unsupported};
}
io_error start_list(resolved_file source, memory_resource* resource,
                    io_completion<directory_entry_list> completion) {
  auto* transport = dynamic_cast<backend_transport*>(source.backend.get());
  return transport
             ? transport->target().list(std::move(source.path), resource, std::move(completion))
             : io_error{io_errc::unsupported};
}
} // namespace detail
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
