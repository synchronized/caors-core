// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/resources/vfs_source.h"
#include "../io/vfs_operations.h"
namespace caors::resources {
result<captured_source> vfs_source::capture() const {
  auto snapshot = routes_->snapshot();
  if (!snapshot)
    return {snapshot.error(), {}};
  return {{}, [snapshot = std::move(snapshot.value)](io::location_view location) {
            return io::detail::vfs_access::resolve(snapshot, location);
          }};
}
} // namespace caors::resources
