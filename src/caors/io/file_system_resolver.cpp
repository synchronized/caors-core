// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/file_system_resolver.h"

#include "caors/io/location.h"

#include <algorithm>
#include <atomic>
#include <utility>

namespace caors::io {
namespace {

// 将后端与宿主凭据放入同一控制块，保证 erase/move 也先释放后端，再释放模块 pin。
struct owned_backend {
  std::shared_ptr<void> lifetime;
  std::shared_ptr<file_system> target;
};

[[nodiscard]] std::string_view string_view_of(const core::string& value) noexcept {
  return {value.data(), value.size()};
}

[[nodiscard]] core::string make_string(std::string_view text, memory_resource* resource) {
  core::string result{allocator<char>{resource}};
  result.assign(text.begin(), text.end());
  return result;
}

[[nodiscard]] core::string normalize_route_path(path_view value, memory_resource* resource) {
  return normalize_path(value, resource).to_string(resource);
}

[[nodiscard]] core::string trim_target_prefix(path_view value, memory_resource* resource) {
  std::string_view prefix = value.string_view();
  while (!prefix.empty() && (prefix.back() == '/' || prefix.back() == '\\')) {
    prefix.remove_suffix(1);
  }

  return make_string(prefix, resource);
}

[[nodiscard]] bool matches_path_prefix(std::string_view prefix, std::string_view path) noexcept {
  if (prefix.empty()) {
    return true;
  }
  if (path == prefix) {
    return true;
  }

  return path.size() > prefix.size() && path.starts_with(prefix) && path[prefix.size()] == '/';
}

[[nodiscard]] bool compare_entries(const directory_entry& lhs,
                                   const directory_entry& rhs) noexcept {
  return lhs.path.string_view() < rhs.path.string_view();
}

void add_or_update_entry(directory_entry_list& entries, std::string_view name,
                         std::string_view full_path, file_kind kind, memory_resource* resource) {
  const auto found =
      std::find_if(entries.begin(), entries.end(),
                   [&](const directory_entry& entry) { return entry.name.string_view() == name; });
  if (found != entries.end()) {
    if (kind == file_kind::directory) {
      found->kind = file_kind::directory;
    }
    return;
  }

  entries.push_back(directory_entry{
      path{name, resource},
      path{full_path, resource},
      kind,
  });
}

} // namespace

file_system_resolver::file_system_resolver(memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      routes_(std::allocate_shared<route_table>(allocator<route_table>{resource_})) {}

memory_resource* file_system_resolver::resource() const noexcept { return resource_; }

error file_system_resolver::mount(uri_view prefix, file_system& target, path_view target_prefix) {
  return mount_owned(prefix, target, target_prefix, {});
}

error file_system_resolver::mount_owned(uri_view prefix, file_system& target,
                                        path_view target_prefix, std::shared_ptr<void> owner) {
  const core::string scheme = make_string(prefix.scheme(), resource());
  const core::string authority = make_string(prefix.authority(), resource());
  const core::string path_prefix = normalize_route_path(prefix.path(), resource());
  if (scheme.empty() && authority.empty() && path_prefix.empty()) {
    return {error_domain::io, errc::invalid_argument};
  }

  auto previous = routes_.load();
  for (;;) {
    auto next = std::allocate_shared<route_table>(allocator<route_table>{resource()}, *previous);
    auto& routes = *next;
    const auto duplicate =
        std::find_if(routes.begin(), routes.end(), [&](const route_entry& entry) {
          return entry.scheme == scheme && entry.authority == authority &&
                 entry.path_prefix == path_prefix;
        });
    if (duplicate != routes.end()) {
      return {error_domain::io, errc::already_exists};
    }

    routes.push_back(route_entry{
        owner,
        detail::next_route_id(),
        scheme,
        authority,
        path_prefix,
        &target,
        trim_target_prefix(target_prefix, resource()),
    });
    std::shared_ptr<const route_table> desired = std::move(next);
    if (routes_.compare_exchange_strong(previous, desired))
      return {};
  }
}

error file_system_resolver::mount(uri_view prefix, std::shared_ptr<file_system> target,
                                  path_view target_prefix, std::shared_ptr<void> lifetime) {
  if (!target) {
    return {error_domain::io, errc::invalid_argument};
  }
  auto* backend = target.get();
  auto owner = std::allocate_shared<owned_backend>(
      allocator<owned_backend>{resource()}, owned_backend{std::move(lifetime), std::move(target)});
  return mount_owned(prefix, *backend, target_prefix, std::move(owner));
}

bool file_system_resolver::unmount(uri_view prefix) {
  const core::string scheme = make_string(prefix.scheme(), resource());
  const core::string authority = make_string(prefix.authority(), resource());
  const core::string path_prefix = normalize_route_path(prefix.path(), resource());

  auto previous = routes_.load();
  for (;;) {
    auto next = std::allocate_shared<route_table>(allocator<route_table>{resource()}, *previous);
    const auto found = std::find_if(next->begin(), next->end(), [&](const route_entry& entry) {
      return entry.scheme == scheme && entry.authority == authority &&
             entry.path_prefix == path_prefix;
    });
    if (found == next->end())
      return false;
    next->erase(found);
    std::shared_ptr<const route_table> desired = std::move(next);
    if (routes_.compare_exchange_strong(previous, desired))
      return true;
  }
}

void file_system_resolver::clear() {
  auto empty = std::allocate_shared<route_table>(allocator<route_table>{resource()});
  routes_.store(std::move(empty));
}

std::size_t file_system_resolver::mount_count() const noexcept { return routes_.load()->size(); }

bool file_system_resolver::exists(path_view location) const {
  const auto table = routes_.load();
  const auto& routes = *table;
  const uri_view parsed{location};
  const route_entry* entry = find_route(routes, parsed);
  return entry != nullptr && entry->target != nullptr &&
         entry->target->exists(resolve_path(*entry, parsed));
}

file_system_capabilities file_system_resolver::capabilities() const noexcept {
  const auto table = routes_.load();
  const auto& routes = *table;
  file_system_capabilities result{};

  for (const route_entry& entry : routes) {
    if (entry.target == nullptr) {
      continue;
    }

    const file_system_capabilities target = entry.target->capabilities();
    result.read = result.read || target.read;
    result.write = result.write || target.write;
    result.status = result.status || target.status;
    result.list = result.list || target.list;
    result.watch = result.watch || target.watch;
    result.seek = result.seek || target.seek;
  }

  return result;
}

file_status_result file_system_resolver::status(path_view location) const {
  const auto table = routes_.load();
  const auto& routes = *table;
  const uri_view parsed{location};
  const route_entry* entry = find_route(routes, parsed);
  if (entry == nullptr || entry->target == nullptr) {
    return fail<file_status>({error_domain::io, errc::not_found});
  }

  return entry->target->status(resolve_path(*entry, parsed));
}

directory_entry_list_result file_system_resolver::list(path_view directory,
                                                       memory_resource* result_resource) {
  const auto table = routes_.load();
  const auto& routes = *table;
  const uri_view parsed{directory};
  const route_entry* entry = find_route(routes, parsed);
  if (entry == nullptr || entry->target == nullptr) {
    return fail<directory_entry_list>({error_domain::io, errc::not_found});
  }

  directory_entry_list_result target_entries =
      entry->target->list(resolve_path(*entry, parsed), result_resource);
  if (target_entries.failed()) {
    return target_entries;
  }

  directory_entry_list entries{allocator<directory_entry>{result_resource}};
  for (const directory_entry& target_entry : *target_entries) {
    const std::string_view name = target_entry.name.string_view();
    if (name.empty()) {
      continue;
    }

    const core::string full_path =
        join_location(directory, name, result_resource).to_string(result_resource);
    add_or_update_entry(entries, name, full_path, target_entry.kind, result_resource);
  }

  std::sort(entries.begin(), entries.end(), compare_entries);
  return ok(std::move(entries));
}

byte_buffer_result file_system_resolver::read_all(path_view location,
                                                  memory_resource* result_resource) {
  const auto table = routes_.load();
  const auto& routes = *table;
  const uri_view parsed{location};
  const route_entry* entry = find_route(routes, parsed);
  if (entry == nullptr || entry->target == nullptr) {
    return fail<byte_buffer>({error_domain::io, errc::not_found});
  }

  return entry->target->read_all(resolve_path(*entry, parsed), result_resource);
}

input_stream_result file_system_resolver::open_read(path_view location,
                                                    memory_resource* result_resource) {
  const auto table = routes_.load();
  const auto& routes = *table;
  const uri_view parsed{location};
  const route_entry* entry = find_route(routes, parsed);
  if (entry == nullptr || entry->target == nullptr) {
    return fail<input_stream_ptr>({error_domain::io, errc::not_found});
  }

  return entry->target->open_read(resolve_path(*entry, parsed), result_resource);
}

output_stream_result file_system_resolver::open_write(path_view location,
                                                      memory_resource* result_resource) {
  const auto table = routes_.load();
  const auto& routes = *table;
  const uri_view parsed{location};
  const route_entry* entry = find_route(routes, parsed);
  if (entry == nullptr || entry->target == nullptr) {
    return fail<output_stream_ptr>({error_domain::io, errc::not_found});
  }

  return entry->target->open_write(resolve_path(*entry, parsed), result_resource);
}

error file_system_resolver::write_all(path_view location, std::span<const std::byte> bytes) {
  const auto table = routes_.load();
  const auto& routes = *table;
  const uri_view parsed{location};
  const route_entry* entry = find_route(routes, parsed);
  if (entry == nullptr || entry->target == nullptr) {
    return {error_domain::io, errc::not_found};
  }

  return entry->target->write_all(resolve_path(*entry, parsed), bytes);
}

const file_system_resolver::route_entry*
file_system_resolver::find_route(const route_table& entries, uri_view location) const {
  const core::string normalized_path =
      normalize_path(location.path(), resource()).to_string(resource());
  const std::string_view path = string_view_of(normalized_path);

  const route_entry* result = nullptr;
  for (const route_entry& entry : entries) {
    if (string_view_of(entry.scheme) != location.scheme() ||
        string_view_of(entry.authority) != location.authority() ||
        !matches_path_prefix(string_view_of(entry.path_prefix), path)) {
      continue;
    }

    const std::size_t score =
        entry.scheme.size() + entry.authority.size() + entry.path_prefix.size();
    const std::size_t result_score =
        result != nullptr
            ? result->scheme.size() + result->authority.size() + result->path_prefix.size()
            : 0;
    if (result == nullptr || score > result_score) {
      result = &entry;
    }
  }

  return result;
}

core::string file_system_resolver::resolve_path(const route_entry& entry, uri_view location) const {
  const core::string normalized_path =
      normalize_path(location.path(), resource()).to_string(resource());
  std::string_view suffix = string_view_of(normalized_path);
  if (!entry.path_prefix.empty()) {
    suffix = suffix.size() == entry.path_prefix.size()
                 ? std::string_view{}
                 : suffix.substr(entry.path_prefix.size() + 1);
  }

  if (entry.target_prefix.empty()) {
    return make_string(suffix, resource());
  }
  if (suffix.empty()) {
    return make_string(string_view_of(entry.target_prefix), resource());
  }

  core::string result = make_string(string_view_of(entry.target_prefix), resource());
  result.push_back('/');
  result.append(suffix.begin(), suffix.end());
  return result;
}

result<resolved_file> file_system_resolver::resolve_owned(path_view location) const {
  const auto table = routes_.load();
  const uri_view parsed{location};
  const auto* entry = find_route(*table, parsed);
  if (!entry)
    return fail<resolved_file>({error_domain::io, errc::not_found});
  if (!entry->owner)
    return fail<resolved_file>({error_domain::io, errc::unsupported});
  auto target_path = resolve_path(*entry, parsed);
  if (entry->target->routes_paths()) {
    auto nested = entry->target->resolve_owned(target_path);
    if (!nested)
      return nested;
    auto owner = std::allocate_shared<owned_backend>(allocator<owned_backend>{resource()},
                                                     owned_backend{entry->owner, nested->backend});
    nested->backend = std::shared_ptr<file_system>{owner, owner->target.get()};
    return nested;
  }
  return ok(resolved_file{std::shared_ptr<file_system>{entry->owner, entry->target},
                          path{path_view{target_path}, resource()}, entry->source_id});
}

result<std::shared_ptr<file_system>>
file_system_resolver::freeze_routes(detail::snapshot_context& context,
                                    std::shared_ptr<void>) const {
  const auto table = routes_.load();
  auto frozen = std::allocate_shared<file_system_resolver>(
      allocator<file_system_resolver>{resource()}, resource());
  auto entries = std::allocate_shared<route_table>(allocator<route_table>{resource()}, *table);
  for (auto& entry : *entries) {
    // 借用边先拒绝，捕获过程中不触碰没有寿命凭据的后端。
    if (!entry.owner)
      return fail<std::shared_ptr<file_system>>({error_domain::io, errc::unsupported});
    auto child = context.capture(*entry.target, entry.owner);
    if (!child)
      return fail<std::shared_ptr<file_system>>(child.error());
    auto owner = std::allocate_shared<owned_backend>(
        allocator<owned_backend>{resource()}, owned_backend{entry.owner, std::move(child.value)});
    entry.target = owner->target.get();
    entry.owner = std::move(owner);
  }
  frozen->routes_.store(std::move(entries));
  return ok(std::static_pointer_cast<file_system>(std::move(frozen)));
}

} // namespace caors::io
