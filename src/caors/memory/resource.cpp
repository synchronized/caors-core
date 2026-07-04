// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/resource.h"

#include "caors/config.h"
#include "memory_internal.h"

#include <atomic>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>

namespace caors::memory::detail {

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

} // namespace caors::memory::detail

namespace caors::memory {

void* memory_resource::try_allocate(std::size_t size, std::size_t alignment) noexcept {
  if (!detail::is_power_of_two(alignment)) {
    return nullptr;
  }

  return do_try_allocate(detail::normalize_size(size), alignment);
}

void* memory_resource::allocate(std::size_t size, std::size_t alignment) {
  detail::validate_alignment_or_fail(alignment);

  void* ptr = do_try_allocate(detail::normalize_size(size), alignment);
  if (ptr == nullptr) {
    detail::fail_bad_alloc();
  }

  return ptr;
}

void memory_resource::deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept {
  if (ptr == nullptr) {
    return;
  }

  do_deallocate(ptr, detail::normalize_size(size), alignment);
}

bool memory_resource::is_equal(const memory_resource& other) const noexcept {
  return this == &other || do_is_equal(other);
}

void* system_memory_resource::do_try_allocate(std::size_t size, std::size_t alignment) noexcept {
  if (detail::needs_aligned_new(alignment)) {
    return ::operator new(size, std::align_val_t{alignment}, std::nothrow);
  }

  return ::operator new(size, std::nothrow);
}

void system_memory_resource::do_deallocate(void* ptr, std::size_t, std::size_t alignment) noexcept {
  if (detail::needs_aligned_new(alignment)) {
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
    detail::fail_invalid_argument("callback memory resource callbacks must not be null");
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

bool operator==(const memory_resource& lhs, const memory_resource& rhs) noexcept {
  return lhs.is_equal(rhs);
}

bool operator!=(const memory_resource& lhs, const memory_resource& rhs) noexcept {
  return !(lhs == rhs);
}

memory_resource& get_system_memory_resource() noexcept { return detail::system_resource; }

memory_resource* get_default_memory_resource() noexcept {
  return &detail::resolve_resource(nullptr);
}

memory_resource* set_default_memory_resource(memory_resource* resource) noexcept {
  memory_resource* old = detail::default_resource.exchange(resource, std::memory_order_acq_rel);
  return old != nullptr ? old : &detail::system_resource;
}

void* try_allocate(std::size_t size, std::size_t alignment, memory_resource* resource) noexcept {
  return detail::resolve_resource(resource).try_allocate(size, alignment);
}

void* allocate(std::size_t size, std::size_t alignment, memory_resource* resource) {
  return detail::resolve_resource(resource).allocate(size, alignment);
}

void deallocate(void* ptr, std::size_t size, std::size_t alignment,
                memory_resource* resource) noexcept {
  detail::resolve_resource(resource).deallocate(ptr, size, alignment);
}

} // namespace caors::memory
