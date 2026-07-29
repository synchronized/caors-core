// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/stack.h"

#include "detail/internal.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

namespace caors::memory {
namespace {

constexpr std::size_t maximum_cached_capacity = 1024 * 1024;

} // namespace

struct stack_resource::chunk {
  chunk* next = nullptr;
  chunk* previous = nullptr;
  void* memory = nullptr;
  std::size_t capacity = 0;
  std::size_t offset = 0;
  bool large = false;
};

stack_resource::stack_resource(std::size_t initial_capacity, memory_resource* upstream)
    : upstream_(&detail::resolve_resource(upstream)),
      initial_capacity_(detail::normalize_size(initial_capacity)),
      next_capacity_(initial_capacity_) {}

stack_resource::~stack_resource() {
  assert(active_depth_ == 0 && "stack_resource 析构时仍存在活动作用域");
  release_all();
}

memory_resource* stack_resource::upstream_resource() const noexcept { return upstream_; }

std::size_t stack_resource::initial_capacity() const noexcept { return initial_capacity_; }

stack_resource::marker stack_resource::mark() noexcept {
  ++active_depth_;
  ++statistics_.mark_count;
  const std::uint64_t parent_id = active_marker_id_;
  ++marker_sequence_;
  if (marker_sequence_ == 0) {
    ++marker_sequence_;
  }
  active_marker_id_ = marker_sequence_;
  return marker{this,
                current_,
                current_ != nullptr ? current_->offset : 0,
                statistics_.bytes_in_use,
                active_depth_,
                generation_,
                active_marker_id_,
                parent_id};
}

void stack_resource::rewind(marker position) noexcept {
  const bool valid = position.owner_ == this && position.generation_ == generation_ &&
                     position.depth_ == active_depth_ && position.id_ == active_marker_id_ &&
                     active_depth_ != 0;
  assert(valid && "stack_resource marker 无效或未按后进先出顺序回滚");
  if (!valid) {
    return;
  }

  auto* marked_chunk = static_cast<chunk*>(position.chunk_);
  if (marked_chunk != nullptr) {
    marked_chunk->offset = position.offset_;
  }

  chunk* item = marked_chunk != nullptr ? marked_chunk->next : chunks_;
  while (item != nullptr) {
    chunk* next = item->next;
    if (item->large || item->capacity > maximum_cached_capacity) {
      release_chunk(item);
    } else {
      item->offset = 0;
    }
    item = next;
  }

  current_ = marked_chunk;
  statistics_.bytes_in_use = position.bytes_in_use_;
  ++statistics_.rewind_count;
  active_marker_id_ = position.parent_id_;
  --active_depth_;
}

void stack_resource::reset() noexcept {
  const bool valid = active_depth_ == 0;
  assert(valid && "stack_resource::reset 不能在活动作用域中调用");
  if (!valid) {
    return;
  }

  release_all();
  next_capacity_ = initial_capacity_;
  active_marker_id_ = 0;
  ++generation_;
  ++statistics_.reset_count;
}

void stack_resource::trim() noexcept {
  const bool valid = active_depth_ == 0;
  assert(valid && "stack_resource::trim 不能在活动作用域中调用");
  if (!valid) {
    return;
  }

  if (chunks_ != nullptr && (chunks_->large || chunks_->capacity > maximum_cached_capacity)) {
    release_all();
    next_capacity_ = initial_capacity_;
    active_marker_id_ = 0;
    ++generation_;
    ++statistics_.trim_count;
    return;
  }

  chunk* item = chunks_ != nullptr ? chunks_->next : nullptr;
  while (item != nullptr) {
    chunk* next = item->next;
    release_chunk(item);
    item = next;
  }

  if (chunks_ != nullptr) {
    chunks_->offset = 0;
    chunks_->next = nullptr;
    chunks_->previous = nullptr;
  }
  tail_ = chunks_;
  current_ = nullptr;
  next_capacity_ = initial_capacity_;
  active_marker_id_ = 0;
  statistics_.bytes_in_use = 0;
  ++generation_;
  ++statistics_.trim_count;
}

std::size_t stack_resource::active_scope_count() const noexcept { return active_depth_; }

stack_resource_statistics stack_resource::statistics() const noexcept { return statistics_; }

void stack_resource::reset_statistics() noexcept {
  const std::size_t bytes_in_use = statistics_.bytes_in_use;
  const std::size_t chunk_count = statistics_.chunk_count;
  statistics_ = {};
  statistics_.bytes_in_use = bytes_in_use;
  statistics_.peak_bytes_in_use = bytes_in_use;
  statistics_.chunk_count = chunk_count;
  statistics_.peak_chunk_count = chunk_count;
}

void* stack_resource::do_try_allocate(std::size_t size, std::size_t alignment) noexcept {
  chunk* candidate = current_ != nullptr ? current_ : chunks_;
  for (; candidate != nullptr; candidate = candidate->next) {
    if (void* result = try_allocate_from(*candidate, size, alignment); result != nullptr) {
      current_ = candidate;
      return result;
    }
  }

  std::size_t required = 0;
  if (!detail::try_checked_add(size, alignment - 1, required)) {
    return nullptr;
  }

  const bool large = required > next_capacity_;
  const std::size_t capacity = large ? required : next_capacity_;
  if (!try_add_chunk(capacity, large)) {
    return nullptr;
  }

  if (large) {
    ++statistics_.large_allocation_count;
  }
  current_ = tail_;
  return try_allocate_from(*current_, size, alignment);
}

void stack_resource::do_deallocate(void*, std::size_t, std::size_t) noexcept {}

bool stack_resource::do_is_equal(const memory_resource&) const noexcept { return false; }

bool stack_resource::try_add_chunk(std::size_t capacity, bool large) noexcept {
  auto* added = static_cast<chunk*>(upstream_->try_allocate(sizeof(chunk), alignof(chunk)));
  if (added == nullptr) {
    return false;
  }

  void* memory = upstream_->try_allocate(capacity, default_memory_alignment);
  if (memory == nullptr) {
    upstream_->deallocate(added, sizeof(chunk), alignof(chunk));
    return false;
  }

  added->next = nullptr;
  added->previous = tail_;
  added->memory = memory;
  added->capacity = capacity;
  added->offset = 0;
  added->large = large;

  if (tail_ != nullptr) {
    tail_->next = added;
  } else {
    chunks_ = added;
  }
  tail_ = added;
  ++statistics_.chunk_count;
  statistics_.peak_chunk_count = std::max(statistics_.peak_chunk_count, statistics_.chunk_count);

  if (!large) {
    if (next_capacity_ <= std::numeric_limits<std::size_t>::max() / 2) {
      next_capacity_ *= 2;
    } else {
      next_capacity_ = std::numeric_limits<std::size_t>::max();
    }
  }

  return true;
}

void* stack_resource::try_allocate_from(chunk& target, std::size_t size,
                                        std::size_t alignment) noexcept {
  const auto base = reinterpret_cast<std::uintptr_t>(target.memory);
  std::uintptr_t address = 0;
  if (!detail::try_checked_add_address(base, target.offset, address)) {
    return nullptr;
  }

  std::uintptr_t aligned = 0;
  if (!detail::try_align_up_address(address, alignment, aligned)) {
    return nullptr;
  }

  const std::size_t aligned_offset = static_cast<std::size_t>(aligned - base);
  std::size_t next_offset = 0;
  if (!detail::try_checked_add(aligned_offset, size, next_offset) ||
      next_offset > target.capacity) {
    return nullptr;
  }

  const std::size_t previous_offset = target.offset;
  target.offset = next_offset;
  statistics_.bytes_in_use += next_offset - previous_offset;
  statistics_.peak_bytes_in_use = std::max(statistics_.peak_bytes_in_use, statistics_.bytes_in_use);
  return reinterpret_cast<void*>(aligned);
}

void stack_resource::release_chunk(chunk* target) noexcept {
  if (target->previous != nullptr) {
    target->previous->next = target->next;
  } else {
    chunks_ = target->next;
  }
  if (target->next != nullptr) {
    target->next->previous = target->previous;
  } else {
    tail_ = target->previous;
  }

  upstream_->deallocate(target->memory, target->capacity, default_memory_alignment);
  upstream_->deallocate(target, sizeof(chunk), alignof(chunk));
  --statistics_.chunk_count;
}

void stack_resource::release_all() noexcept {
  chunk* item = chunks_;
  while (item != nullptr) {
    chunk* next = item->next;
    upstream_->deallocate(item->memory, item->capacity, default_memory_alignment);
    upstream_->deallocate(item, sizeof(chunk), alignof(chunk));
    item = next;
  }

  chunks_ = nullptr;
  tail_ = nullptr;
  current_ = nullptr;
  statistics_.bytes_in_use = 0;
  statistics_.chunk_count = 0;
}

stack_scope::stack_scope(stack_resource& resource) noexcept
    : resource_(&resource), marker_(resource.mark()) {}

stack_scope::~stack_scope() { resource_->rewind(marker_); }

memory_resource* stack_scope::resource() noexcept { return resource_; }

stack_resource& library_temporary_resource() noexcept {
  thread_local stack_resource resource;
  return resource;
}

} // namespace caors::memory
