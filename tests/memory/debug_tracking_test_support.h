// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/core/containers.h"
#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <array>
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

struct retained_memory_state {
  alignas(64) std::array<std::byte, 512> storage{};
  std::size_t allocate_count = 0;
  std::size_t deallocate_count = 0;
  std::size_t allocated_size = 0;
  std::size_t allocated_alignment = 0;
  void* allocated_ptr = nullptr;
  std::size_t deallocated_size = 0;
  std::size_t deallocated_alignment = 0;
  void* deallocated_ptr = nullptr;
};

inline void* retained_allocate(std::size_t size, std::size_t alignment, void* user_data) {
  auto* state = static_cast<retained_memory_state*>(user_data);
  ++state->allocate_count;
  state->allocated_size = size;
  state->allocated_alignment = alignment;
  state->allocated_ptr = state->storage.data();
  if (size > state->storage.size() || alignment > alignof(retained_memory_state)) {
    return nullptr;
  }
  return state->allocated_ptr;
}

inline void retained_deallocate(void* ptr, std::size_t size, std::size_t alignment,
                                void* user_data) {
  auto* state = static_cast<retained_memory_state*>(user_data);
  ++state->deallocate_count;
  state->deallocated_ptr = ptr;
  state->deallocated_size = size;
  state->deallocated_alignment = alignment;
}

struct violation_capture {
  caors::memory::debug_tracking_resource* resource = nullptr;
  std::array<caors::memory::memory_debug_violation_kind, 4> kinds{};
  std::size_t count = 0;
  bool reentered = false;
};

inline void capture_violation(const caors::memory::memory_debug_violation& violation,
                              void* user_data) noexcept {
  auto* capture = static_cast<violation_capture*>(user_data);
  if (capture->count < capture->kinds.size()) {
    capture->kinds[capture->count] = violation.kind;
  }
  ++capture->count;

  if (capture->resource != nullptr && !capture->reentered) {
    capture->reentered = true;
    capture->resource->set_violation_callback(nullptr);
  }
}

} // namespace caors::tests::debug_tracking
