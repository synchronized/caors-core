// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io/vfs.h"
#include "caors/core/detail/shared_snapshot.h"
#include "caors/io/location.h"
#include "route_identity.h"
#include "vfs_operations.h"
#include <algorithm>
#include <mutex>
namespace caors::io {
namespace detail {
struct vfs_route {
  core::string scheme;
  core::string authority;
  core::string prefix;
  core::string target_prefix;
  std::shared_ptr<backend> target;
  std::uint64_t source_id;
};
struct vfs_routes {
  explicit vfs_routes(memory_resource* value)
      : resource(value), entries(allocator<vfs_route>{value}) {}
  memory_resource* resource;
  core::vector<vfs_route> entries;
};
struct vfs_state {
  explicit vfs_state(memory_resource* value)
      : resource(value ? value : get_default_memory_resource()),
        routes(std::allocate_shared<vfs_routes>(allocator<vfs_routes>{resource}, resource)) {}
  memory_resource* resource;
  std::mutex update;
  ::caors::detail::shared_snapshot<const vfs_routes> routes;
};
io_error start_write(backend_location source, byte_buffer bytes,
                     io_completion<std::uint64_t> completion) {
  return source.backend->write(std::move(source.path), std::move(bytes), std::move(completion));
}
io_error start_stat(backend_location source, io_completion<file_status> completion) {
  return source.backend->stat(std::move(source.path), std::move(completion));
}
io_error start_list(backend_location source, memory_resource* resource,
                    io_completion<directory_entry_list> completion) {
  return source.backend->list(std::move(source.path), resource, std::move(completion));
}
} // namespace detail
namespace {
// 控制块先销毁后端，再释放宿主凭据；复制、移除挂载均保留该顺序。
struct owned_backend {
  std::shared_ptr<void> lifetime;
  std::shared_ptr<backend> target;
};
core::string make_string(std::string_view value, memory_resource* resource) {
  core::string result{allocator<char>{resource}};
  result.assign(value.begin(), value.end());
  return result;
}
bool same_prefix(const detail::vfs_route& entry, uri_view uri, const core::string& prefix) {
  return entry.scheme == uri.scheme() && entry.authority == uri.authority() &&
         entry.prefix == prefix;
}
} // namespace
vfs::vfs(memory_resource* resource) : routes_(std::make_shared<detail::vfs_state>(resource)) {}
error vfs::mount(uri_view prefix, std::shared_ptr<backend> target, path_view target_prefix,
                 std::shared_ptr<void> lifetime) {
  if (!target)
    return {error_domain::io, errc::invalid_argument};
  auto* resource = routes_->resource;
  auto normalized = normalize_path(prefix.path(), resource).to_string(resource);
  if (prefix.scheme().empty() && prefix.authority().empty() && normalized.empty())
    return {error_domain::io, errc::invalid_argument};
  auto owner = std::allocate_shared<owned_backend>(
      allocator<owned_backend>{resource}, owned_backend{std::move(lifetime), std::move(target)});
  auto retained = std::shared_ptr<backend>{owner, owner->target.get()};
  auto destination = target_prefix.string_view();
  while (!destination.empty() && (destination.back() == '/' || destination.back() == '\\'))
    destination.remove_suffix(1);
  std::shared_ptr<const detail::vfs_routes> previous;
  {
    std::lock_guard lock{routes_->update};
    previous = routes_->routes.load();
    for (const auto& entry : previous->entries)
      if (same_prefix(entry, prefix, normalized))
        return {error_domain::io, errc::already_exists};
    auto next = std::allocate_shared<detail::vfs_routes>(allocator<detail::vfs_routes>{resource},
                                                         *previous);
    next->entries.push_back({make_string(prefix.scheme(), resource),
                             make_string(prefix.authority(), resource), std::move(normalized),
                             make_string(destination, resource), std::move(retained),
                             detail::next_route_id()});
    routes_->routes.store(std::move(next));
  }
  return {};
}
bool vfs::unmount(uri_view prefix) {
  auto* resource = routes_->resource;
  auto normalized = normalize_path(prefix.path(), resource).to_string(resource);
  std::shared_ptr<const detail::vfs_routes> previous;
  {
    std::lock_guard lock{routes_->update};
    previous = routes_->routes.load();
    auto next = std::allocate_shared<detail::vfs_routes>(allocator<detail::vfs_routes>{resource},
                                                         *previous);
    const auto entry =
        std::find_if(next->entries.begin(), next->entries.end(),
                     [&](const auto& value) { return same_prefix(value, prefix, normalized); });
    if (entry == next->entries.end())
      return false;
    next->entries.erase(entry);
    routes_->routes.store(std::move(next));
  }
  return true;
}
void vfs::clear() {
  auto empty = std::allocate_shared<detail::vfs_routes>(
      allocator<detail::vfs_routes>{routes_->resource}, routes_->resource);
  std::shared_ptr<const detail::vfs_routes> previous;
  {
    std::lock_guard lock{routes_->update};
    previous = routes_->routes.load();
    routes_->routes.store(std::move(empty));
  }
}
std::size_t vfs::mount_count() const noexcept { return routes_->routes.load()->entries.size(); }
result<vfs_snapshot> vfs::snapshot() const { return {{}, vfs_snapshot{routes_->routes.load()}}; }
result<backend_location> vfs_snapshot::resolve(path_view location) const {
  if (!routes_)
    return {{error_domain::io, errc::invalid_argument}, {}};
  const uri_view uri{location};
  auto normalized = normalize_path(uri.path(), routes_->resource).to_string(routes_->resource);
  const std::string_view value{normalized.data(), normalized.size()};
  const detail::vfs_route* selected = nullptr;
  for (const auto& entry : routes_->entries) {
    const std::string_view prefix{entry.prefix.data(), entry.prefix.size()};
    if (entry.scheme != uri.scheme() || entry.authority != uri.authority())
      continue;
    if (!prefix.empty() && value != prefix &&
        !(value.size() > prefix.size() && value.starts_with(prefix) && value[prefix.size()] == '/'))
      continue;
    if (!selected || entry.prefix.size() > selected->prefix.size())
      selected = &entry;
  }
  if (!selected)
    return {{error_domain::io, errc::not_found}, {}};
  auto suffix = value;
  if (!selected->prefix.empty())
    suffix = suffix.size() == selected->prefix.size() ? std::string_view{}
                                                      : suffix.substr(selected->prefix.size() + 1);
  auto target = selected->target_prefix;
  if (!target.empty() && !suffix.empty())
    target.push_back('/');
  target.append(suffix.begin(), suffix.end());
  return {{}, {selected->target, path{path_view{target}, routes_->resource}, selected->source_id}};
}
} // namespace caors::io
