// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/resources/vfs_source.h"
namespace caors::resources {
result<std::shared_ptr<io::file_system>>
vfs_source::freeze_routes(io::detail::snapshot_context& context) const {
  auto snapshot = routes_->snapshot();
  if (!snapshot)
    return {snapshot.error(), {}};
  auto adapter = io::detail::legacy_snapshot(std::move(snapshot.value));
  return context.capture(*adapter, adapter);
}
} // namespace caors::resources
