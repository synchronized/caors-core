// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"

#include "caors/config.h"
#include "caors/core/containers.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>

namespace caors {
namespace {

bool is_power_of_two(std::size_t value) noexcept {
  return value != 0 && (value & (value - 1)) == 0;
}

std::size_t normalize_size(std::size_t size) noexcept { return size == 0 ? 1 : size; }

[[noreturn]] void fail_bad_alloc() {
#if CAORS_HAS_EXCEPTIONS
  throw std::bad_alloc();
#else
  std::abort();
#endif
}

[[noreturn]] void fail_invalid_argument(const char* message) {
#if CAORS_HAS_EXCEPTIONS
  throw std::invalid_argument(message);
#else
  (void)message;
  std::abort();
#endif
}

void validate_alignment_or_fail(std::size_t alignment) {
  if (!is_power_of_two(alignment)) {
    fail_invalid_argument("memory alignment must be a non-zero power of two");
  }
}

bool needs_aligned_new(std::size_t alignment) noexcept {
  return alignment > config::default_new_alignment;
}

bool try_checked_add(std::size_t lhs, std::size_t rhs, std::size_t& result) noexcept {
  if (lhs > std::numeric_limits<std::size_t>::max() - rhs) {
    return false;
  }

  result = lhs + rhs;
  return true;
}

bool try_checked_multiply(std::size_t lhs, std::size_t rhs, std::size_t& result) noexcept {
  if (lhs != 0 && rhs > std::numeric_limits<std::size_t>::max() / lhs) {
    return false;
  }

  result = lhs * rhs;
  return true;
}

bool try_checked_add_address(std::uintptr_t lhs, std::size_t rhs, std::uintptr_t& result) noexcept {
  const auto rhs_address = static_cast<std::uintptr_t>(rhs);
  if (rhs_address != rhs || lhs > std::numeric_limits<std::uintptr_t>::max() - rhs_address) {
    return false;
  }

  result = lhs + rhs_address;
  return true;
}

bool try_align_up(std::size_t value, std::size_t alignment, std::size_t& result) noexcept {
  std::size_t adjusted = 0;
  if (!try_checked_add(value, alignment - 1, adjusted)) {
    return false;
  }

  result = adjusted & ~(alignment - 1);
  return true;
}

std::size_t align_up_or_fail(std::size_t value, std::size_t alignment) {
  std::size_t result = 0;
  if (!try_align_up(value, alignment, result)) {
    fail_bad_alloc();
  }

  return result;
}

bool try_align_up_address(std::uintptr_t value, std::size_t alignment,
                          std::uintptr_t& result) noexcept {
  const auto mask = static_cast<std::uintptr_t>(alignment - 1);
  if (value > std::numeric_limits<std::uintptr_t>::max() - mask) {
    return false;
  }

  result = (value + mask) & ~mask;
  return true;
}

void update_peak(std::atomic<std::size_t>& peak, std::size_t value) noexcept {
  std::size_t current = peak.load(std::memory_order_relaxed);
  while (current < value && !peak.compare_exchange_weak(current, value, std::memory_order_relaxed,
                                                        std::memory_order_relaxed)) {
  }
}

void subtract_saturating(std::atomic<std::size_t>& value, std::size_t amount) noexcept {
  std::size_t current = value.load(std::memory_order_relaxed);
  std::size_t desired = 0;
  do {
    desired = current > amount ? current - amount : 0;
  } while (!value.compare_exchange_weak(current, desired, std::memory_order_relaxed,
                                        std::memory_order_relaxed));
}

system_memory_resource system_resource;
std::atomic<memory_resource*> default_resource{nullptr};

memory_resource& resolve_resource(memory_resource* resource) noexcept {
  if (resource != nullptr) {
    return *resource;
  }

  memory_resource* current = default_resource.load(std::memory_order_acquire);
  return current != nullptr ? *current : system_resource;
}

} // namespace

struct monotonic_resource::chunk {
  chunk* next = nullptr;
  void* memory = nullptr;
  std::size_t capacity = 0;
  std::size_t offset = 0;
};

struct fixed_block_pool::chunk {
  chunk* next = nullptr;
  void* memory = nullptr;
  std::size_t bytes = 0;
};

struct fixed_block_pool::free_block {
  free_block* next = nullptr;
};

struct debug_tracking_state {
  explicit debug_tracking_state(memory_resource* upstream_resource) noexcept
      : upstream(upstream_resource) {}

  memory_resource* upstream = nullptr;
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

void* memory_resource::try_allocate(std::size_t size, std::size_t alignment) noexcept {
  if (!is_power_of_two(alignment)) {
    return nullptr;
  }

  return do_try_allocate(normalize_size(size), alignment);
}

void* memory_resource::allocate(std::size_t size, std::size_t alignment) {
  validate_alignment_or_fail(alignment);

  void* ptr = do_try_allocate(normalize_size(size), alignment);
  if (ptr == nullptr) {
    fail_bad_alloc();
  }

  return ptr;
}

void memory_resource::deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept {
  if (ptr == nullptr) {
    return;
  }

  do_deallocate(ptr, normalize_size(size), alignment);
}

bool memory_resource::is_equal(const memory_resource& other) const noexcept {
  return this == &other || do_is_equal(other);
}

void* system_memory_resource::do_try_allocate(std::size_t size, std::size_t alignment) noexcept {
  if (needs_aligned_new(alignment)) {
    return ::operator new(size, std::align_val_t{alignment}, std::nothrow);
  }

  return ::operator new(size, std::nothrow);
}

void system_memory_resource::do_deallocate(void* ptr, std::size_t, std::size_t alignment) noexcept {
  if (needs_aligned_new(alignment)) {
    ::operator delete(ptr, std::align_val_t{alignment});
    return;
  }

  ::operator delete(ptr);
}

bool system_memory_resource::do_is_equal(const memory_resource& other) const noexcept {
  return dynamic_cast<const system_memory_resource*>(&other) != nullptr;
}

callback_memory_resource::callback_memory_resource(memory_allocate_fn allocate_fn,
                                                   memory_deallocate_fn deallocate_fn,
                                                   void* user_data)
    : allocate_fn_(allocate_fn), deallocate_fn_(deallocate_fn), user_data_(user_data) {
  if (allocate_fn_ == nullptr || deallocate_fn_ == nullptr) {
    fail_invalid_argument("callback memory resource callbacks must not be null");
  }
}

memory_allocate_fn callback_memory_resource::allocate_callback() const noexcept {
  return allocate_fn_;
}

memory_deallocate_fn callback_memory_resource::deallocate_callback() const noexcept {
  return deallocate_fn_;
}

void* callback_memory_resource::user_data() const noexcept { return user_data_; }

void* callback_memory_resource::do_try_allocate(std::size_t size, std::size_t alignment) noexcept {
#if CAORS_HAS_EXCEPTIONS
  try {
    return allocate_fn_(size, alignment, user_data_);
  } catch (...) {
    return nullptr;
  }
#else
  return allocate_fn_(size, alignment, user_data_);
#endif
}

void callback_memory_resource::do_deallocate(void* ptr, std::size_t size,
                                             std::size_t alignment) noexcept {
  deallocate_fn_(ptr, size, alignment, user_data_);
}

bool callback_memory_resource::do_is_equal(const memory_resource& other) const noexcept {
  const auto* rhs = dynamic_cast<const callback_memory_resource*>(&other);
  return rhs != nullptr && allocate_fn_ == rhs->allocate_fn_ &&
         deallocate_fn_ == rhs->deallocate_fn_ && user_data_ == rhs->user_data_;
}

tracking_resource::tracking_resource(memory_resource* upstream) noexcept
    : upstream_(&resolve_resource(upstream)) {}

memory_resource* tracking_resource::upstream_resource() const noexcept { return upstream_; }

memory_statistics tracking_resource::statistics() const noexcept {
  return memory_statistics{
      allocation_count_.load(std::memory_order_relaxed),
      deallocation_count_.load(std::memory_order_relaxed),
      bytes_allocated_.load(std::memory_order_relaxed),
      bytes_deallocated_.load(std::memory_order_relaxed),
      bytes_in_use_.load(std::memory_order_relaxed),
      peak_bytes_in_use_.load(std::memory_order_relaxed),
  };
}

void tracking_resource::reset_statistics() noexcept {
  allocation_count_.store(0, std::memory_order_relaxed);
  deallocation_count_.store(0, std::memory_order_relaxed);
  bytes_allocated_.store(0, std::memory_order_relaxed);
  bytes_deallocated_.store(0, std::memory_order_relaxed);
  bytes_in_use_.store(0, std::memory_order_relaxed);
  peak_bytes_in_use_.store(0, std::memory_order_relaxed);
}

void* tracking_resource::do_try_allocate(std::size_t size, std::size_t alignment) noexcept {
  void* ptr = upstream_->try_allocate(size, alignment);
  if (ptr == nullptr) {
    return nullptr;
  }

  allocation_count_.fetch_add(1, std::memory_order_relaxed);
  bytes_allocated_.fetch_add(size, std::memory_order_relaxed);
  const std::size_t in_use = bytes_in_use_.fetch_add(size, std::memory_order_relaxed) + size;
  update_peak(peak_bytes_in_use_, in_use);

  return ptr;
}

void tracking_resource::do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept {
  upstream_->deallocate(ptr, size, alignment);

  deallocation_count_.fetch_add(1, std::memory_order_relaxed);
  bytes_deallocated_.fetch_add(size, std::memory_order_relaxed);
  subtract_saturating(bytes_in_use_, size);
}

bool tracking_resource::do_is_equal(const memory_resource&) const noexcept { return false; }

debug_tracking_resource::debug_tracking_resource(memory_resource* upstream)
    : state_(new debug_tracking_state(&resolve_resource(upstream))) {}

debug_tracking_resource::~debug_tracking_resource() { delete state_; }

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
  validate_alignment_or_fail(alignment);

  void* ptr = try_allocate_recorded(size, alignment, label, location);
  if (ptr == nullptr) {
    fail_bad_alloc();
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

  const std::size_t normalized_size = normalize_size(size);
  if (record.size != normalized_size || record.alignment != alignment) {
    state_->mismatched_deallocation_count.fetch_add(1, std::memory_order_relaxed);
  }

  state_->upstream->deallocate(ptr, record.size, record.alignment);

  state_->deallocation_count.fetch_add(1, std::memory_order_relaxed);
  state_->bytes_deallocated.fetch_add(record.size, std::memory_order_relaxed);
  state_->live_allocation_count.fetch_sub(1, std::memory_order_relaxed);
  subtract_saturating(state_->bytes_in_use, record.size);
}

bool debug_tracking_resource::do_is_equal(const memory_resource&) const noexcept { return false; }

void* debug_tracking_resource::try_allocate_recorded(std::size_t size, std::size_t alignment,
                                                     const char* label,
                                                     std::source_location location) noexcept {
  if (!is_power_of_two(alignment)) {
    return nullptr;
  }

  const std::size_t normalized_size = normalize_size(size);
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
  update_peak(state_->peak_bytes_in_use, in_use);

  return ptr;
}

monotonic_resource::monotonic_resource(std::size_t initial_capacity, memory_resource* upstream)
    : upstream_(&resolve_resource(upstream)), initial_capacity_(normalize_size(initial_capacity)),
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
      if (!try_checked_add_address(base, current->offset, current_address)) {
        continue;
      }

      std::uintptr_t aligned = 0;
      if (!try_align_up_address(current_address, alignment, aligned)) {
        continue;
      }

      const std::size_t aligned_offset = static_cast<std::size_t>(aligned - base);
      std::size_t next_offset = 0;
      if (!try_checked_add(aligned_offset, size, next_offset)) {
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
  if (!try_checked_add(size, alignment - 1, required)) {
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

fixed_block_pool::fixed_block_pool(std::size_t block_size, std::size_t block_alignment,
                                   std::size_t blocks_per_chunk, memory_resource* upstream)
    : upstream_(&resolve_resource(upstream)), blocks_per_chunk_(normalize_size(blocks_per_chunk)) {
  validate_alignment_or_fail(block_alignment);

  block_alignment_ = std::max(block_alignment, alignof(free_block));
  block_size_ =
      align_up_or_fail(std::max(normalize_size(block_size), sizeof(free_block)), block_alignment_);
}

fixed_block_pool::~fixed_block_pool() { release(); }

memory_resource* fixed_block_pool::upstream_resource() const noexcept { return upstream_; }

std::size_t fixed_block_pool::block_size() const noexcept { return block_size_; }

std::size_t fixed_block_pool::block_alignment() const noexcept { return block_alignment_; }

std::size_t fixed_block_pool::blocks_per_chunk() const noexcept { return blocks_per_chunk_; }

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
}

void* fixed_block_pool::do_try_allocate(std::size_t size, std::size_t alignment) noexcept {
  if (size > block_size_ || alignment > block_alignment_) {
    return nullptr;
  }

  if (free_blocks_ == nullptr && !try_add_chunk()) {
    return nullptr;
  }

  free_block* block = free_blocks_;
  free_blocks_ = block->next;
  return block;
}

void fixed_block_pool::do_deallocate(void* ptr, std::size_t, std::size_t) noexcept {
  auto* block = static_cast<free_block*>(ptr);
  block->next = free_blocks_;
  free_blocks_ = block;
}

bool fixed_block_pool::do_is_equal(const memory_resource&) const noexcept { return false; }

bool fixed_block_pool::try_add_chunk() noexcept {
  std::size_t bytes = 0;
  if (!try_checked_multiply(block_size_, blocks_per_chunk_, bytes)) {
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

  auto* memory = static_cast<unsigned char*>(new_chunk->memory);
  for (std::size_t index = 0; index < blocks_per_chunk_; ++index) {
    auto* block = reinterpret_cast<free_block*>(memory + index * block_size_);
    block->next = free_blocks_;
    free_blocks_ = block;
  }

  return true;
}

bool operator==(const memory_resource& lhs, const memory_resource& rhs) noexcept {
  return lhs.is_equal(rhs);
}

bool operator!=(const memory_resource& lhs, const memory_resource& rhs) noexcept {
  return !(lhs == rhs);
}

memory_resource& get_system_memory_resource() noexcept { return system_resource; }

memory_resource* get_default_memory_resource() noexcept { return &resolve_resource(nullptr); }

memory_resource* set_default_memory_resource(memory_resource* resource) noexcept {
  memory_resource* old = default_resource.exchange(resource, std::memory_order_acq_rel);
  return old != nullptr ? old : &system_resource;
}

void* try_allocate(std::size_t size, std::size_t alignment, memory_resource* resource) noexcept {
  return resolve_resource(resource).try_allocate(size, alignment);
}

void* allocate(std::size_t size, std::size_t alignment, memory_resource* resource) {
  return resolve_resource(resource).allocate(size, alignment);
}

void deallocate(void* ptr, std::size_t size, std::size_t alignment,
                memory_resource* resource) noexcept {
  resolve_resource(resource).deallocate(ptr, size, alignment);
}

}
