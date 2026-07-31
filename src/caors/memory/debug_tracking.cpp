// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/debug_tracking.h"

#include "caors/core/containers.h"
#include "detail/internal.h"

#include <array>
#include <cstring>
#include <mutex>

namespace caors::memory {
namespace {

struct debug_allocation_record {
  memory_debug_record public_record;
  void* raw_ptr = nullptr;
  std::size_t raw_size = 0;
  std::size_t raw_alignment = 0;
};

struct debug_allocation_layout {
  void* raw_ptr = nullptr;
  void* user_ptr = nullptr;
  std::size_t raw_size = 0;
  std::size_t raw_alignment = 0;
};

[[nodiscard]] bool try_compute_raw_size(std::size_t size, std::size_t alignment,
                                        std::size_t guard_size, std::size_t& raw_size) noexcept {
  if (guard_size == 0) {
    raw_size = size;
    return true;
  }

  std::size_t guard_bytes = 0;
  std::size_t guarded_size = 0;
  return detail::try_checked_multiply(guard_size, 2, guard_bytes) &&
         detail::try_checked_add(size, guard_bytes, guarded_size) &&
         detail::try_checked_add(guarded_size, alignment - 1, raw_size);
}

[[nodiscard]] bool try_compute_user_pointer(void* raw_ptr, std::size_t guard_size,
                                            std::size_t alignment, void*& user_ptr) noexcept {
  if (guard_size == 0) {
    user_ptr = raw_ptr;
    return true;
  }

  std::uintptr_t candidate = 0;
  std::uintptr_t aligned = 0;
  return detail::try_checked_add_address(reinterpret_cast<std::uintptr_t>(raw_ptr), guard_size,
                                         candidate) &&
         detail::try_align_up_address(candidate, alignment, aligned) &&
         (user_ptr = reinterpret_cast<void*>(aligned), true);
}

void fill_bytes(void* ptr, std::size_t size, std::byte pattern) noexcept {
  if (size != 0) {
    std::memset(ptr, std::to_integer<unsigned char>(pattern), size);
  }
}

[[nodiscard]] bool bytes_match(const void* ptr, std::size_t size, std::byte pattern) noexcept {
  const auto* bytes = static_cast<const std::byte*>(ptr);
  for (std::size_t index = 0; index < size; ++index) {
    if (bytes[index] != pattern) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::byte* front_guard(const debug_allocation_record& record,
                                     std::size_t guard_size) noexcept {
  return static_cast<std::byte*>(record.public_record.ptr) - guard_size;
}

[[nodiscard]] std::byte* back_guard(const debug_allocation_record& record) noexcept {
  return static_cast<std::byte*>(record.public_record.ptr) + record.public_record.size;
}

} // namespace

struct debug_tracking_state {
  debug_tracking_state(memory_resource* upstream_resource, memory_resource* storage_resource,
                       memory_debug_options debug_options) noexcept
      : upstream(upstream_resource), storage(storage_resource), options(debug_options),
        allocations(hash_map<void*, debug_allocation_record>::allocator_type{storage_resource}) {}

  memory_resource* upstream = nullptr;
  memory_resource* storage = nullptr;
  memory_debug_options options;
  mutable std::mutex mutex;
  hash_map<void*, debug_allocation_record> allocations;
  debug_tracking_resource::violation_fn violation_callback = nullptr;
  void* violation_user_data = nullptr;
  std::atomic<std::size_t> allocation_count{0};
  std::atomic<std::size_t> deallocation_count{0};
  std::atomic<std::size_t> bytes_allocated{0};
  std::atomic<std::size_t> bytes_deallocated{0};
  std::atomic<std::size_t> bytes_in_use{0};
  std::atomic<std::size_t> peak_bytes_in_use{0};
  std::atomic<std::size_t> live_allocation_count{0};
  std::atomic<std::size_t> invalid_deallocation_count{0};
  std::atomic<std::size_t> mismatched_deallocation_count{0};
  std::atomic<std::size_t> front_guard_corruption_count{0};
  std::atomic<std::size_t> back_guard_corruption_count{0};
};

debug_tracking_resource::debug_tracking_resource(memory_resource* upstream)
    : debug_tracking_resource(upstream, memory_debug_options{}) {}

debug_tracking_resource::debug_tracking_resource(memory_resource* upstream,
                                                 memory_debug_options options)
    : state_(nullptr) {
  memory_resource& resolved = detail::resolve_resource(upstream);
  memory_resource& storage = library_resource();
  state_ = make<debug_tracking_state>(&storage, &resolved, &storage, options);
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
      state_->front_guard_corruption_count.load(std::memory_order_relaxed),
      state_->back_guard_corruption_count.load(std::memory_order_relaxed),
  };
}

std::size_t debug_tracking_resource::live_allocation_count() const noexcept {
  return state_->live_allocation_count.load(std::memory_order_relaxed);
}

std::size_t debug_tracking_resource::live_bytes() const noexcept {
  return state_->bytes_in_use.load(std::memory_order_relaxed);
}

bool debug_tracking_resource::has_leaks() const noexcept { return live_allocation_count() != 0; }

memory_debug_options debug_tracking_resource::options() const noexcept { return state_->options; }

void debug_tracking_resource::set_violation_callback(violation_fn callback,
                                                     void* user_data) noexcept {
  std::lock_guard lock(state_->mutex);
  state_->violation_callback = callback;
  state_->violation_user_data = user_data;
}

void debug_tracking_resource::for_each_live_allocation(record_fn callback,
                                                       void* user_data) const noexcept {
  if (callback == nullptr) {
    return;
  }

  std::lock_guard lock(state_->mutex);
  for (const auto& allocation : state_->allocations) {
    callback(allocation.second.public_record, user_data);
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
  debug_allocation_record record;
  violation_fn violation_callback = nullptr;
  void* violation_user_data = nullptr;
  bool found = false;

  {
    std::lock_guard lock(state_->mutex);
    violation_callback = state_->violation_callback;
    violation_user_data = state_->violation_user_data;
    const auto position = state_->allocations.find(ptr);
    if (position != state_->allocations.end()) {
      record = position->second;
      state_->allocations.erase(position);
      found = true;
    }
  }

  if (!found) {
    state_->invalid_deallocation_count.fetch_add(1, std::memory_order_relaxed);
    if (violation_callback != nullptr) {
      memory_debug_violation violation;
      violation.kind = memory_debug_violation_kind::invalid_deallocation;
      violation.allocation.ptr = ptr;
      violation.supplied_size = size;
      violation.supplied_alignment = alignment;
      violation_callback(violation, violation_user_data);
    }
    return;
  }

  std::array<memory_debug_violation, 3> violations;
  std::size_t violation_count = 0;
  const std::size_t normalized_size = detail::normalize_size(size);
  if (record.public_record.size != normalized_size || record.public_record.alignment != alignment) {
    state_->mismatched_deallocation_count.fetch_add(1, std::memory_order_relaxed);
    violations[violation_count++] = memory_debug_violation{
        memory_debug_violation_kind::mismatched_deallocation,
        record.public_record,
        size,
        alignment,
    };
  }

  const std::size_t guard_size = state_->options.guard_size;
  if (guard_size != 0 && !bytes_match(front_guard(record, guard_size), guard_size,
                                      state_->options.front_guard_pattern)) {
    state_->front_guard_corruption_count.fetch_add(1, std::memory_order_relaxed);
    violations[violation_count++] = memory_debug_violation{
        memory_debug_violation_kind::front_guard_corruption,
        record.public_record,
        size,
        alignment,
    };
  }

  if (guard_size != 0 &&
      !bytes_match(back_guard(record), guard_size, state_->options.back_guard_pattern)) {
    state_->back_guard_corruption_count.fetch_add(1, std::memory_order_relaxed);
    violations[violation_count++] = memory_debug_violation{
        memory_debug_violation_kind::back_guard_corruption,
        record.public_record,
        size,
        alignment,
    };
  }

  if (state_->options.fill_on_deallocate) {
    fill_bytes(record.public_record.ptr, record.public_record.size,
               state_->options.deallocation_fill_pattern);
  }

  state_->upstream->deallocate(record.raw_ptr, record.raw_size, record.raw_alignment);

  state_->deallocation_count.fetch_add(1, std::memory_order_relaxed);
  state_->bytes_deallocated.fetch_add(record.public_record.size, std::memory_order_relaxed);
  state_->live_allocation_count.fetch_sub(1, std::memory_order_relaxed);
  detail::subtract_saturating(state_->bytes_in_use, record.public_record.size);

  if (violation_callback != nullptr) {
    for (std::size_t index = 0; index < violation_count; ++index) {
      violation_callback(violations[index], violation_user_data);
    }
  }
}

bool debug_tracking_resource::do_is_equal(const memory_resource&) const noexcept { return false; }

void* debug_tracking_resource::try_allocate_recorded(std::size_t size, std::size_t alignment,
                                                     const char* label,
                                                     std::source_location location) noexcept {
  if (!detail::is_power_of_two(alignment)) {
    return nullptr;
  }

  const std::size_t normalized_size = detail::normalize_size(size);
  std::size_t raw_size = 0;
  if (!try_compute_raw_size(normalized_size, alignment, state_->options.guard_size, raw_size)) {
    return nullptr;
  }

  debug_allocation_layout layout;
  layout.raw_size = raw_size;
  layout.raw_alignment = alignment;
  layout.raw_ptr = state_->upstream->try_allocate(raw_size, alignment);
  if (layout.raw_ptr == nullptr ||
      !try_compute_user_pointer(layout.raw_ptr, state_->options.guard_size, alignment,
                                layout.user_ptr)) {
    if (layout.raw_ptr != nullptr) {
      state_->upstream->deallocate(layout.raw_ptr, raw_size, alignment);
    }
    return nullptr;
  }

  debug_allocation_record record{
      memory_debug_record{
          layout.user_ptr,
          normalized_size,
          alignment,
          label,
          location.file_name(),
          location.function_name(),
          location.line(),
          location.column(),
      },
      layout.raw_ptr,
      layout.raw_size,
      layout.raw_alignment,
  };

  const std::size_t guard_size = state_->options.guard_size;
  if (guard_size != 0) {
    fill_bytes(front_guard(record, guard_size), guard_size, state_->options.front_guard_pattern);
    fill_bytes(back_guard(record), guard_size, state_->options.back_guard_pattern);
  }
  if (state_->options.fill_on_allocate) {
    fill_bytes(record.public_record.ptr, normalized_size, state_->options.allocation_fill_pattern);
  }

  bool inserted = false;
#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    {
      std::lock_guard lock(state_->mutex);
      inserted = state_->allocations.emplace(record.public_record.ptr, record).second;
    }
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    state_->upstream->deallocate(record.raw_ptr, record.raw_size, record.raw_alignment);
    return nullptr;
  }
#endif

  if (!inserted) {
    state_->upstream->deallocate(record.raw_ptr, record.raw_size, record.raw_alignment);
    return nullptr;
  }

  state_->allocation_count.fetch_add(1, std::memory_order_relaxed);
  state_->bytes_allocated.fetch_add(normalized_size, std::memory_order_relaxed);
  state_->live_allocation_count.fetch_add(1, std::memory_order_relaxed);
  const std::size_t in_use =
      state_->bytes_in_use.fetch_add(normalized_size, std::memory_order_relaxed) + normalized_size;
  detail::update_peak(state_->peak_bytes_in_use, in_use);

  return record.public_record.ptr;
}

} // namespace caors::memory
