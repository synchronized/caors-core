// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/monotonic.h"

#include "memory_internal.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace caors {

struct monotonic_resource::chunk {
  chunk* next = nullptr;
  void* memory = nullptr;
  std::size_t capacity = 0;
  std::size_t offset = 0;
};

monotonic_resource::monotonic_resource(std::size_t initial_capacity, memory_resource* upstream)
    : upstream_(&detail::resolve_resource(upstream)),
      initial_capacity_(detail::normalize_size(initial_capacity)),
      next_capacity_(initial_capacity_) {}

monotonic_resource::~monotonic_resource() { reset(); }

memory_resource* monotonic_resource::upstream_resource() const noexcept { return upstream_; }

std::size_t monotonic_resource::initial_capacity() const noexcept { return initial_capacity_; }

void monotonic_resource::reset() noexcept {
  chunk* current = chunks_;
  while (current != nullptr) {
    chunk* next = current->next;
    upstream_->deallocate(current->memory, current->capacity, default_memory_alignment);
    upstream_->deallocate(current, sizeof(chunk), alignof(chunk));
    current = next;
  }

  chunks_ = nullptr;
  next_capacity_ = initial_capacity_;
}

void* monotonic_resource::do_try_allocate(std::size_t size, std::size_t alignment) noexcept {
  const auto allocate_from_chunks = [this, size, alignment]() noexcept -> void* {
    for (chunk* current = chunks_; current != nullptr; current = current->next) {
      const auto base = reinterpret_cast<std::uintptr_t>(current->memory);
      std::uintptr_t current_address = 0;
      if (!detail::try_checked_add_address(base, current->offset, current_address)) {
        continue;
      }

      std::uintptr_t aligned = 0;
      if (!detail::try_align_up_address(current_address, alignment, aligned)) {
        continue;
      }

      const std::size_t aligned_offset = static_cast<std::size_t>(aligned - base);
      std::size_t next_offset = 0;
      if (!detail::try_checked_add(aligned_offset, size, next_offset)) {
        continue;
      }

      if (next_offset <= current->capacity) {
        current->offset = next_offset;
        return reinterpret_cast<void*>(aligned);
      }
    }

    return nullptr;
  };

  if (void* ptr = allocate_from_chunks(); ptr != nullptr) {
    return ptr;
  }

  std::size_t required = 0;
  if (!detail::try_checked_add(size, alignment - 1, required)) {
    return nullptr;
  }

  const std::size_t preferred_capacity = std::max(required, next_capacity_);
  if (!try_add_chunk(preferred_capacity)) {
    if (preferred_capacity == required || !try_add_chunk(required)) {
      return nullptr;
    }
  }

  return allocate_from_chunks();
}

void monotonic_resource::do_deallocate(void*, std::size_t, std::size_t) noexcept {}

bool monotonic_resource::do_is_equal(const memory_resource&) const noexcept { return false; }

bool monotonic_resource::try_add_chunk(std::size_t capacity) noexcept {
  auto* new_chunk = static_cast<chunk*>(upstream_->try_allocate(sizeof(chunk), alignof(chunk)));
  if (new_chunk == nullptr) {
    return false;
  }

  void* memory = upstream_->try_allocate(capacity, default_memory_alignment);
  if (memory == nullptr) {
    upstream_->deallocate(new_chunk, sizeof(chunk), alignof(chunk));
    return false;
  }

  new_chunk->memory = memory;
  new_chunk->capacity = capacity;
  new_chunk->offset = 0;
  new_chunk->next = chunks_;
  chunks_ = new_chunk;

  if (next_capacity_ <= std::numeric_limits<std::size_t>::max() / 2) {
    next_capacity_ = std::max(next_capacity_ * 2, capacity);
  } else {
    next_capacity_ = std::numeric_limits<std::size_t>::max();
  }

  return true;
}

} // namespace caors
