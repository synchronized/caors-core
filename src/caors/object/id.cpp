// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object/id.h"

#include <atomic>
#include <limits>

namespace caors::objects {
namespace {

std::atomic<std::uint64_t> next_object_id{1};

} // namespace

object_id generate_object_id() noexcept {
  std::uint64_t current = next_object_id.load(std::memory_order_relaxed);
  while (current != 0) {
    const std::uint64_t next =
        current == std::numeric_limits<std::uint64_t>::max() ? 0 : current + 1U;
    if (next_object_id.compare_exchange_weak(current, next, std::memory_order_relaxed,
                                             std::memory_order_relaxed)) {
      return object_id{current};
    }
  }

  return object_id::invalid();
}

} // namespace caors::objects
