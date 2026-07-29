// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/fixed_block_pool.h"

#include "detail/internal.h"

#include <algorithm>

namespace caors::memory {

struct fixed_block_pool::chunk {
  chunk* next = nullptr;
  void* memory = nullptr;
  std::size_t bytes = 0;
};

struct fixed_block_pool::free_block {
  free_block* next = nullptr;
};

fixed_block_pool::fixed_block_pool(std::size_t block_size, std::size_t block_alignment,
                                   std::size_t blocks_per_chunk, memory_resource* upstream)
    : upstream_(&detail::resolve_resource(upstream)),
      blocks_per_chunk_(detail::normalize_size(blocks_per_chunk)) {
  detail::validate_alignment_or_fail(block_alignment);

  block_alignment_ = std::max(block_alignment, alignof(free_block));
  block_size_ = detail::align_up_or_fail(
      std::max(detail::normalize_size(block_size), sizeof(free_block)), block_alignment_);
}

fixed_block_pool::~fixed_block_pool() { release(); }

memory_resource* fixed_block_pool::upstream_resource() const noexcept { return upstream_; }

std::size_t fixed_block_pool::block_size() const noexcept { return block_size_; }

std::size_t fixed_block_pool::block_alignment() const noexcept { return block_alignment_; }

std::size_t fixed_block_pool::blocks_per_chunk() const noexcept { return blocks_per_chunk_; }

fixed_block_pool_statistics fixed_block_pool::statistics() const noexcept { return statistics_; }

void fixed_block_pool::reset_statistics() noexcept {
  const std::size_t used_block_count = statistics_.used_block_count;
  const std::size_t total_block_count = statistics_.total_block_count;
  const std::size_t free_block_count = statistics_.free_block_count;
  const std::size_t chunk_count = statistics_.chunk_count;
  statistics_ = {};
  statistics_.used_block_count = used_block_count;
  statistics_.peak_used_block_count = used_block_count;
  statistics_.total_block_count = total_block_count;
  statistics_.free_block_count = free_block_count;
  statistics_.chunk_count = chunk_count;
}

void fixed_block_pool::release() noexcept {
  chunk* current = chunks_;
  while (current != nullptr) {
    chunk* next = current->next;
    upstream_->deallocate(current->memory, current->bytes, block_alignment_);
    upstream_->deallocate(current, sizeof(chunk), alignof(chunk));
    current = next;
  }

  chunks_ = nullptr;
  free_blocks_ = nullptr;
  statistics_.used_block_count = 0;
  statistics_.total_block_count = 0;
  statistics_.free_block_count = 0;
  statistics_.chunk_count = 0;
  ++statistics_.release_count;
}

void* fixed_block_pool::do_try_allocate(std::size_t size, std::size_t alignment) noexcept {
  if (size > block_size_ || alignment > block_alignment_) {
    ++statistics_.rejected_request_count;
    return nullptr;
  }

  if (free_blocks_ == nullptr && !try_add_chunk()) {
    ++statistics_.allocation_failure_count;
    return nullptr;
  }

  free_block* block = free_blocks_;
  free_blocks_ = block->next;
  ++statistics_.used_block_count;
  --statistics_.free_block_count;
  ++statistics_.allocation_count;
  statistics_.peak_used_block_count =
      std::max(statistics_.peak_used_block_count, statistics_.used_block_count);
  return block;
}

void fixed_block_pool::do_deallocate(void* ptr, std::size_t, std::size_t) noexcept {
  auto* block = static_cast<free_block*>(ptr);
  block->next = free_blocks_;
  free_blocks_ = block;
  --statistics_.used_block_count;
  ++statistics_.free_block_count;
  ++statistics_.deallocation_count;
}

bool fixed_block_pool::do_is_equal(const memory_resource&) const noexcept { return false; }

bool fixed_block_pool::try_add_chunk() noexcept {
  std::size_t bytes = 0;
  if (!detail::try_checked_multiply(block_size_, blocks_per_chunk_, bytes)) {
    return false;
  }

  auto* new_chunk = static_cast<chunk*>(upstream_->try_allocate(sizeof(chunk), alignof(chunk)));
  if (new_chunk == nullptr) {
    return false;
  }

  void* chunk_memory = upstream_->try_allocate(bytes, block_alignment_);
  if (chunk_memory == nullptr) {
    upstream_->deallocate(new_chunk, sizeof(chunk), alignof(chunk));
    return false;
  }

  new_chunk->memory = chunk_memory;
  new_chunk->bytes = bytes;
  new_chunk->next = chunks_;
  chunks_ = new_chunk;
  ++statistics_.chunk_count;
  ++statistics_.chunk_allocation_count;
  statistics_.total_block_count += blocks_per_chunk_;
  statistics_.free_block_count += blocks_per_chunk_;

  auto* memory = static_cast<unsigned char*>(new_chunk->memory);
  for (std::size_t index = 0; index < blocks_per_chunk_; ++index) {
    auto* block = reinterpret_cast<free_block*>(memory + index * block_size_);
    block->next = free_blocks_;
    free_blocks_ = block;
  }

  return true;
}

} // namespace caors::memory
