// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/size_class_pool.h"

#include "detail/internal.h"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <new>

namespace caors::memory {
namespace {

struct free_block {
  free_block* next = nullptr;
};

struct pool_chunk {
  pool_chunk* next = nullptr;
  void* memory = nullptr;
  std::size_t bytes = 0;
};

struct pool_class {
  pool_chunk* chunks = nullptr;
  free_block* free_blocks = nullptr;
  std::size_t block_size = 0;
};

constexpr std::size_t max_class_count = std::numeric_limits<std::size_t>::digits;

[[nodiscard]] bool valid_options(const size_class_pool_options& options) noexcept {
  return detail::is_power_of_two(options.smallest_block_size) &&
         detail::is_power_of_two(options.largest_block_size) &&
         options.smallest_block_size <= options.largest_block_size &&
         options.smallest_block_size >= sizeof(free_block) && options.blocks_per_chunk != 0 &&
         options.target_chunk_size != 0 && detail::is_power_of_two(options.pooled_alignment) &&
         options.pooled_alignment >= alignof(free_block);
}

[[nodiscard]] std::size_t compute_class_count(const size_class_pool_options& options) noexcept {
  return static_cast<std::size_t>(std::countr_zero(options.largest_block_size) -
                                  std::countr_zero(options.smallest_block_size)) +
         1;
}

} // namespace

struct size_class_pool_resource::state {
  state(size_class_pool_options configured_options, memory_resource& upstream_resource) noexcept
      : options(configured_options), upstream(&upstream_resource),
        class_count(compute_class_count(configured_options)) {
    std::size_t block_size = options.smallest_block_size;
    for (std::size_t index = 0; index < class_count; ++index) {
      classes[index].block_size = block_size;
      block_size *= index + 1 < class_count ? 2 : 1;
    }
  }

  size_class_pool_options options{};
  memory_resource* upstream = nullptr;
  memory_resource* storage = nullptr;
  std::array<pool_class, max_class_count> classes{};
  std::size_t class_count = 0;
  size_class_pool_statistics statistics{};
};

size_class_pool_resource::size_class_pool_resource(memory_resource* upstream)
    : size_class_pool_resource(size_class_pool_options{}, upstream) {}

size_class_pool_resource::size_class_pool_resource(size_class_pool_options options,
                                                   memory_resource* upstream) {
  if (!valid_options(options)) {
    detail::fail_invalid_argument("invalid size class pool options");
  }

  memory_resource& storage = library_resource();
  void* memory = storage.allocate(sizeof(state), alignof(state));
  state_ = ::new (memory) state(options, detail::resolve_resource(upstream));
  state_->storage = &storage;
}

size_class_pool_resource::~size_class_pool_resource() {
  if (state_ == nullptr) {
    return;
  }

  release();
  memory_resource* storage = state_->storage;
  state_->~state();
  storage->deallocate(state_, sizeof(state), alignof(state));
}

memory_resource* size_class_pool_resource::upstream_resource() const noexcept {
  return state_->upstream;
}

const size_class_pool_options& size_class_pool_resource::options() const noexcept {
  return state_->options;
}

size_class_pool_statistics size_class_pool_resource::statistics() const noexcept {
  return state_->statistics;
}

void size_class_pool_resource::reset_statistics() noexcept {
  const std::size_t chunk_count = state_->statistics.chunk_count;
  const std::size_t requested = state_->statistics.pooled_requested_bytes_in_use;
  const std::size_t capacity = state_->statistics.pooled_capacity_bytes_in_use;
  const std::size_t reserved = state_->statistics.reserved_bytes;
  state_->statistics = {};
  state_->statistics.chunk_count = chunk_count;
  state_->statistics.pooled_requested_bytes_in_use = requested;
  state_->statistics.pooled_capacity_bytes_in_use = capacity;
  state_->statistics.peak_pooled_capacity_bytes_in_use = capacity;
  state_->statistics.reserved_bytes = reserved;
}

void size_class_pool_resource::release() noexcept {
  for (std::size_t index = 0; index < state_->class_count; ++index) {
    pool_class& target = state_->classes[index];
    pool_chunk* current = target.chunks;
    while (current != nullptr) {
      pool_chunk* next = current->next;
      state_->upstream->deallocate(current->memory, current->bytes,
                                   state_->options.pooled_alignment);
      state_->upstream->deallocate(current, sizeof(pool_chunk), alignof(pool_chunk));
      current = next;
    }
    target.chunks = nullptr;
    target.free_blocks = nullptr;
  }

  state_->statistics.chunk_count = 0;
  state_->statistics.pooled_requested_bytes_in_use = 0;
  state_->statistics.pooled_capacity_bytes_in_use = 0;
  state_->statistics.reserved_bytes = 0;
  ++state_->statistics.release_count;
}

void* size_class_pool_resource::do_try_allocate(std::size_t size, std::size_t alignment) noexcept {
  const std::size_t required = std::max(size, alignment);
  if (required > state_->options.largest_block_size ||
      alignment > state_->options.pooled_alignment) {
    void* ptr = state_->upstream->try_allocate(size, alignment);
    if (ptr == nullptr) {
      ++state_->statistics.allocation_failure_count;
      return nullptr;
    }
    ++state_->statistics.fallback_allocation_count;
    return ptr;
  }

  const std::size_t class_size =
      std::max(state_->options.smallest_block_size, std::bit_ceil(required));
  const std::size_t class_index = static_cast<std::size_t>(
      std::countr_zero(class_size) - std::countr_zero(state_->options.smallest_block_size));
  pool_class& target = state_->classes[class_index];

  if (target.free_blocks == nullptr) {
    const std::size_t target_block_count =
        std::max<std::size_t>(1, state_->options.target_chunk_size / target.block_size);
    const std::size_t block_count = std::min(state_->options.blocks_per_chunk, target_block_count);
    std::size_t bytes = 0;
    if (!detail::try_checked_multiply(target.block_size, block_count, bytes)) {
      ++state_->statistics.allocation_failure_count;
      return nullptr;
    }

    auto* chunk = static_cast<pool_chunk*>(
        state_->upstream->try_allocate(sizeof(pool_chunk), alignof(pool_chunk)));
    if (chunk == nullptr) {
      ++state_->statistics.allocation_failure_count;
      return nullptr;
    }

    void* memory = state_->upstream->try_allocate(bytes, state_->options.pooled_alignment);
    if (memory == nullptr) {
      state_->upstream->deallocate(chunk, sizeof(pool_chunk), alignof(pool_chunk));
      ++state_->statistics.allocation_failure_count;
      return nullptr;
    }

    chunk->next = target.chunks;
    chunk->memory = memory;
    chunk->bytes = bytes;
    target.chunks = chunk;
    auto* cursor = static_cast<unsigned char*>(memory);
    for (std::size_t block = 0; block < block_count; ++block) {
      auto* item = reinterpret_cast<free_block*>(cursor + block * target.block_size);
      item->next = target.free_blocks;
      target.free_blocks = item;
    }
    ++state_->statistics.chunk_count;
    ++state_->statistics.chunk_allocation_count;
    state_->statistics.reserved_bytes += bytes;
  }

  free_block* block = target.free_blocks;
  target.free_blocks = block->next;
  ++state_->statistics.pooled_allocation_count;
  state_->statistics.pooled_requested_bytes_in_use += size;
  state_->statistics.pooled_capacity_bytes_in_use += target.block_size;
  state_->statistics.peak_pooled_capacity_bytes_in_use =
      std::max(state_->statistics.peak_pooled_capacity_bytes_in_use,
               state_->statistics.pooled_capacity_bytes_in_use);
  return block;
}

void size_class_pool_resource::do_deallocate(void* ptr, std::size_t size,
                                             std::size_t alignment) noexcept {
  const std::size_t required = std::max(size, alignment);
  if (required > state_->options.largest_block_size ||
      alignment > state_->options.pooled_alignment) {
    state_->upstream->deallocate(ptr, size, alignment);
    ++state_->statistics.fallback_deallocation_count;
    return;
  }

  const std::size_t class_size =
      std::max(state_->options.smallest_block_size, std::bit_ceil(required));
  const std::size_t class_index = static_cast<std::size_t>(
      std::countr_zero(class_size) - std::countr_zero(state_->options.smallest_block_size));
  pool_class& target = state_->classes[class_index];
  auto* block = static_cast<free_block*>(ptr);
  block->next = target.free_blocks;
  target.free_blocks = block;
  ++state_->statistics.pooled_deallocation_count;
  state_->statistics.pooled_requested_bytes_in_use -= size;
  state_->statistics.pooled_capacity_bytes_in_use -= target.block_size;
}

bool size_class_pool_resource::do_is_equal(const memory_resource&) const noexcept { return false; }

} // namespace caors::memory
