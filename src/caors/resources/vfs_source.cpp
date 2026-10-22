// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/resources/vfs_source.h"
#include "../io/vfs_operations.h"
namespace caors::resources {
namespace {
class backend_bridge final : public io::file_system {
public:
  explicit backend_bridge(std::shared_ptr<io::backend> target) : target_(std::move(target)) {}
  bool exists(io::path_view) const override { return false; }
  io::byte_buffer_result read_all(io::path_view, memory_resource*) override {
    return {{error_domain::io, errc::unsupported}, {}};
  }
  error write_all(io::path_view, std::span<const std::byte>) override {
    return {error_domain::io, errc::unsupported};
  }
  io::io_error
  start_open_options(io::resolved_file location, io::open_options options,
                     memory_resource* resource,
                     io::io_completion<std::shared_ptr<io::async_file>> completion) override {
    return target_->open(std::move(location.path), options, resource, std::move(completion));
  }
  io::io_error start_open(io::resolved_file location, memory_resource* resource,
                          io::io_completion<std::shared_ptr<io::async_file>> completion) override {
    return target_->open(std::move(location.path), {}, resource, std::move(completion));
  }

private:
  std::shared_ptr<io::backend> target_;
};
class snapshot_bridge final : public io::file_system {
public:
  explicit snapshot_bridge(io::vfs_snapshot value) : snapshot_(std::move(value)) {}
  bool exists(io::path_view) const override { return false; }
  io::byte_buffer_result read_all(io::path_view, memory_resource*) override {
    return {{error_domain::io, errc::unsupported}, {}};
  }
  error write_all(io::path_view, std::span<const std::byte>) override {
    return {error_domain::io, errc::unsupported};
  }
  bool routes_paths() const noexcept override { return true; }
  result<io::resolved_file> resolve_owned(io::path_view path) const override {
    auto value = io::detail::vfs_access::resolve(snapshot_, path);
    if (!value)
      return {value.error(), {}};
    return {{},
            {std::make_shared<backend_bridge>(std::move(value->backend)), std::move(value->path),
             value->source_id}};
  }
  result<std::shared_ptr<io::file_system>>
  freeze_routes(io::detail::snapshot_context&, std::shared_ptr<void> owner) const override {
    if (!owner)
      return {{error_domain::io, errc::unsupported}, {}};
    return {{},
            std::shared_ptr<io::file_system>{std::move(owner), const_cast<snapshot_bridge*>(this)}};
  }

private:
  io::vfs_snapshot snapshot_;
};
} // namespace
result<std::shared_ptr<io::file_system>>
vfs_source::freeze_routes(io::detail::snapshot_context& context) const {
  auto snapshot = routes_->snapshot();
  if (!snapshot)
    return {snapshot.error(), {}};
  auto adapter = std::make_shared<snapshot_bridge>(std::move(snapshot.value));
  return context.capture(*adapter, adapter);
}
} // namespace caors::resources
