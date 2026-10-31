// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/route_snapshot.h"
#include "caors/io/file_system.h"

#include <algorithm>

namespace caors::io {
namespace {
struct owned_root {
  std::shared_ptr<void> lifetime;
  std::shared_ptr<file_system> target;
};
}

legacy::route_provider::~route_provider() = default;
result<route_snapshot> legacy::route_provider::snapshot() const {
  auto* files = dynamic_cast<const file_system*>(this);
  if (!files)
    return fail<route_snapshot>({error_domain::io, errc::invalid_argument});
  detail::snapshot_context context;
  auto root = context.capture(*files, {});
  if (!root)
    return fail<route_snapshot>(root.error());
  return ok(route_snapshot{std::move(root.value)});
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
struct file_system_access {
  static std::uint64_t identity(const file_system& files) { return files.source_instance_; }
};
result<resolved_file> resolve_snapshot(const std::shared_ptr<file_system>& root,
                                       path_view location) {
  if (!root)
    return fail<resolved_file>({error_domain::io, errc::invalid_argument});
  auto* routes = dynamic_cast<const legacy::route_provider*>(root.get());
  if (!routes)
    return ok(resolved_file{root, path{location}, file_system_access::identity(*root)});
  auto file = routes->resolve_owned(location);
  if (!file)
    return file;
  auto held = std::make_shared<owned_root>(owned_root{root, std::move(file->backend)});
  file->backend = std::shared_ptr<file_system>{held, held->target.get()};
  return file;
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
  auto* routes = dynamic_cast<const legacy::route_provider*>(&target);
  auto frozen = routes  ? routes->freeze_routes(*this, owner)
                : owner ? ok(std::shared_ptr<file_system>{owner, const_cast<file_system*>(&target)})
                        : fail<std::shared_ptr<file_system>>({error_domain::io, errc::unsupported});
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
