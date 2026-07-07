// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/core/containers.h"
#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <cstring>

namespace caors::tests::debug_tracking {

struct failing_memory_state {
  int allocate_count = 0;
};

inline void* failed_allocate(std::size_t, std::size_t, void* user_data) {
  auto* state = static_cast<failing_memory_state*>(user_data);
  ++state->allocate_count;
  return nullptr;
}

inline void failed_deallocate(void*, std::size_t, std::size_t, void*) {}

} // namespace caors::tests::debug_tracking
