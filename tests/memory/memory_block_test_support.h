// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <new>
#include <utility>

namespace caors::tests::memory_block {

struct callback_memory_state {
  int allocate_count = 0;
  int deallocate_count = 0;
  std::size_t last_size = 0;
  std::size_t last_alignment = 0;
};

inline void* test_failed_allocate(std::size_t size, std::size_t alignment, void* user_data) {
  auto* state = static_cast<callback_memory_state*>(user_data);
  ++state->allocate_count;
  state->last_size = size;
  state->last_alignment = alignment;
  return nullptr;
}

inline void test_deallocate(void* ptr, std::size_t size, std::size_t alignment, void* user_data) {
  auto* state = static_cast<callback_memory_state*>(user_data);
  ++state->deallocate_count;
  state->last_size = size;
  state->last_alignment = alignment;
  ::operator delete(ptr, std::align_val_t{alignment});
}

} // namespace caors::tests::memory_block
