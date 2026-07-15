// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/debug_tracking.h"

#include "caors/core/containers.h"
#include "detail/internal.h"

#include <mutex>

namespace caors::memory {

struct debug_tracking_state {
  debug_tracking_state(memory_resource* upstream_resource,
                       memory_resource* storage_resource) noexcept
      : upstream(upstream_resource), storage(storage_resource),
        allocations(hash_map<void*, memory_debug_record>::allocator_type{storage_resource}) {}

  memory_resource* upstream = nullptr;
  memory_resource* storage = nullptr;
  mutable std::mutex mutex;
  hash_map<void*, memory_debug_record> allocations;
  std::atomic<std::size_t> allocation_count{0};
  std::atomic<std::size_t> deallocation_count{0};
  std::atomic<std::size_t> bytes_allocated{0};
  std::atomic<std::size_t> bytes_deallocated{0};
  std::atomic<std::size_t> bytes_in_use{0};
  std::atomic<std::size_t> peak_bytes_in_use{0};
  std::atomic<std::size_t> live_allocation_count{0};
  std::atomic<std::size_t> invalid_deallocation_count{0};
  std::atomic<std::size_t> mismatched_deallocation_count{0};
};

debug_tracking_resource::debug_tracking_resource(memory_resource* upstream) : state_(nullptr) {
  memory_resource& resolved = detail::resolve_resource(upstream);
  memory_resource& storage = library_resource();
  state_ = make<debug_tracking_state>(&storage, &resolved, &storage);
}

debug_tracking_resource::~debug_tracking_resource() {
  memory_resource* storage = state_ != nullptr ? state_->storage : nullptr;
  destroy(state_, storage);
}

memory_resource* debug_tracking_resource::upstream_resource() const noexcept {
  return state_->upstream;
}

memory_debug_statistics debug_tracking_resource::debug_statistics() const noexcept {
  return memory_debug_statistics{
      memory_statistics{
          state_->allocation_count.load(std::memory_order_relaxed),
          state_->deallocation_count.load(std::memory_order_relaxed),
          state_->bytes_allocated.load(std::memory_order_relaxed),
          state_->bytes_deallocated.load(std::memory_order_relaxed),
          state_->bytes_in_use.load(std::memory_order_relaxed),
          state_->peak_bytes_in_use.load(std::memory_order_relaxed),
      },
      state_->live_allocation_count.load(std::memory_order_relaxed),
      state_->invalid_deallocation_count.load(std::memory_order_relaxed),
      state_->mismatched_deallocation_count.load(std::memory_order_relaxed),
  };
}

std::size_t debug_tracking_resource::live_allocation_count() const noexcept {
  return state_->live_allocation_count.load(std::memory_order_relaxed);
}

std::size_t debug_tracking_resource::live_bytes() const noexcept {
  return state_->bytes_in_use.load(std::memory_order_relaxed);
}

bool debug_tracking_resource::has_leaks() const noexcept { return live_allocation_count() != 0; }

void debug_tracking_resource::for_each_live_allocation(memory_debug_record_fn callback,
                                                       void* user_data) const noexcept {
  if (callback == nullptr) {
    return;
  }

  std::lock_guard lock(state_->mutex);
  for (const auto& allocation : state_->allocations) {
    callback(allocation.second, user_data);
  }
}

void* debug_tracking_resource::try_allocate_debug(std::size_t size, std::size_t alignment,
                                                  const char* label,
                                                  std::source_location location) noexcept {
  return try_allocate_recorded(size, alignment, label, location);
}

void* debug_tracking_resource::try_allocate_debug(std::size_t size, const char* label,
                                                  std::source_location location) noexcept {
  return try_allocate_debug(size, default_memory_alignment, label, location);
}

void* debug_tracking_resource::allocate_debug(std::size_t size, std::size_t alignment,
                                              const char* label, std::source_location location) {
  detail::validate_alignment_or_fail(alignment);

  void* ptr = try_allocate_recorded(size, alignment, label, location);
  if (ptr == nullptr) {
    detail::fail_bad_alloc();
  }

  return ptr;
}

void* debug_tracking_resource::allocate_debug(std::size_t size, const char* label,
                                              std::source_location location) {
  return allocate_debug(size, default_memory_alignment, label, location);
}

void* debug_tracking_resource::do_try_allocate(std::size_t size, std::size_t alignment) noexcept {
  return try_allocate_recorded(size, alignment, nullptr, std::source_location{});
}

void debug_tracking_resource::do_deallocate(void* ptr, std::size_t size,
                                            std::size_t alignment) noexcept {
  memory_debug_record record;
  bool found = false;

  {
    std::lock_guard lock(state_->mutex);
    const auto position = state_->allocations.find(ptr);
    if (position != state_->allocations.end()) {
      record = position->second;
      state_->allocations.erase(position);
      found = true;
    }
  }

  if (!found) {
    state_->invalid_deallocation_count.fetch_add(1, std::memory_order_relaxed);
    return;
  }

  const std::size_t normalized_size = detail::normalize_size(size);
  if (record.size != normalized_size || record.alignment != alignment) {
    state_->mismatched_deallocation_count.fetch_add(1, std::memory_order_relaxed);
  }

  state_->upstream->deallocate(ptr, record.size, record.alignment);

  state_->deallocation_count.fetch_add(1, std::memory_order_relaxed);
  state_->bytes_deallocated.fetch_add(record.size, std::memory_order_relaxed);
  state_->live_allocation_count.fetch_sub(1, std::memory_order_relaxed);
  detail::subtract_saturating(state_->bytes_in_use, record.size);
}

bool debug_tracking_resource::do_is_equal(const memory_resource&) const noexcept { return false; }

void* debug_tracking_resource::try_allocate_recorded(std::size_t size, std::size_t alignment,
                                                     const char* label,
                                                     std::source_location location) noexcept {
  if (!detail::is_power_of_two(alignment)) {
    return nullptr;
  }

  const std::size_t normalized_size = detail::normalize_size(size);
  void* ptr = state_->upstream->try_allocate(normalized_size, alignment);
  if (ptr == nullptr) {
    return nullptr;
  }

  const memory_debug_record record{
      ptr,
      normalized_size,
      alignment,
      label,
      location.file_name(),
      location.function_name(),
      location.line(),
      location.column(),
  };

  bool inserted = false;
#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    {
      std::lock_guard lock(state_->mutex);
      inserted = state_->allocations.emplace(ptr, record).second;
    }
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    state_->upstream->deallocate(ptr, normalized_size, alignment);
    return nullptr;
  }
#endif

  if (!inserted) {
    state_->upstream->deallocate(ptr, normalized_size, alignment);
    return nullptr;
  }

  state_->allocation_count.fetch_add(1, std::memory_order_relaxed);
  state_->bytes_allocated.fetch_add(normalized_size, std::memory_order_relaxed);
  state_->live_allocation_count.fetch_add(1, std::memory_order_relaxed);
  const std::size_t in_use =
      state_->bytes_in_use.fetch_add(normalized_size, std::memory_order_relaxed) + normalized_size;
  detail::update_peak(state_->peak_bytes_in_use, in_use);

  return ptr;
}

} // namespace caors::memory
