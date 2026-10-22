// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "route_identity.h"
#include <atomic>
namespace caors::io::detail {
std::uint64_t next_route_id() noexcept {
  static std::atomic<std::uint64_t> next{1};
  return next.fetch_add(1, std::memory_order_relaxed);
}
}
