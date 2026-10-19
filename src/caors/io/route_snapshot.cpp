// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/file_system.h"

#include <algorithm>
#include <atomic>

namespace caors::io {
namespace {
struct owned_root {
  std::shared_ptr<void> lifetime;
  std::shared_ptr<file_system> target;
};
}

result<route_snapshot> file_system::snapshot() const {
  detail::snapshot_context context;
  auto root = context.capture(*this, {});
  if (!root)
    return fail<route_snapshot>(root.error());
  return ok(route_snapshot{std::move(root.value)});
}

result<resolved_file> file_system::resolve_owned(path_view) const {
  return fail<resolved_file>({error_domain::io, errc::unsupported});
}

result<std::shared_ptr<file_system>> file_system::freeze_routes(detail::snapshot_context&,
                                                                std::shared_ptr<void> owner) const {
  if (routes_paths() || !owner)
    return fail<std::shared_ptr<file_system>>({error_domain::io, errc::unsupported});
  return ok(std::shared_ptr<file_system>{std::move(owner), const_cast<file_system*>(this)});
}

result<route_snapshot> route_snapshot::capture(std::shared_ptr<file_system> root,
                                               std::shared_ptr<void> lifetime) {
  if (!root)
    return fail<route_snapshot>({error_domain::io, errc::invalid_argument});
  auto owner = std::make_shared<owned_root>(owned_root{std::move(lifetime), root});
  detail::snapshot_context context;
  auto captured = context.capture(*root, owner);
  if (!captured)
    return fail<route_snapshot>(captured.error());
  // 叶根和自定义路由副本同样保留宿主凭据，先析构副本，再释放原根及凭据。
  auto held = std::make_shared<owned_root>(owned_root{std::move(owner), std::move(captured.value)});
  return ok(route_snapshot{std::shared_ptr<file_system>{held, held->target.get()}});
}

result<resolved_file> route_snapshot::resolve(path_view location) const {
  return detail::resolve_snapshot(root_, location);
}

namespace detail {
result<resolved_file> resolve_snapshot(const std::shared_ptr<file_system>& root,
                                       path_view location) {
  if (!root)
    return fail<resolved_file>({error_domain::io, errc::invalid_argument});
  if (!root->routes_paths())
    return ok(resolved_file{root, path{location}, root->source_instance()});
  auto file = root->resolve_owned(location);
  if (!file)
    return file;
  auto held = std::make_shared<owned_root>(owned_root{root, std::move(file->backend)});
  file->backend = std::shared_ptr<file_system>{held, held->target.get()};
  return file;
}
std::uint64_t next_route_id() noexcept {
  static std::atomic<std::uint64_t> next{1};
  return next.fetch_add(1, std::memory_order_relaxed);
}
result<std::shared_ptr<file_system>> snapshot_context::capture(const file_system& target,
                                                               std::shared_ptr<void> owner) {
  if (std::find(active_.begin(), active_.end(), &target) != active_.end())
    return fail<std::shared_ptr<file_system>>({error_domain::io, errc::cycle_detected});
  auto pin = [&](std::shared_ptr<file_system> frozen) {
    if (!owner)
      return frozen;
    auto held = std::make_shared<owned_root>(owned_root{owner, std::move(frozen)});
    return std::shared_ptr<file_system>{held, held->target.get()};
  };
  if (const auto found = captured_.find(&target); found != captured_.end())
    return ok(pin(found->second));
  if (active_.size() >= 256)
    return fail<std::shared_ptr<file_system>>({error_domain::io, errc::recursion_limit});
  active_.push_back(&target);
  struct pop_guard {
    std::vector<const file_system*>& stack;
    ~pop_guard() { stack.pop_back(); }
  } guard{active_};
  auto frozen = target.freeze_routes(*this, owner);
  if (frozen && !frozen.value)
    return fail<std::shared_ptr<file_system>>({error_domain::io, errc::backend_failure});
  if (frozen) {
    captured_.emplace(&target, frozen.value);
    frozen.value = pin(std::move(frozen.value));
  }
  return frozen;
}
} // namespace detail
} // namespace caors::io
