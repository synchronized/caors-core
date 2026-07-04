// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "internal.h"

#include "caors/config.h"

#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace caors::memory::detail {
namespace {

system_memory_resource system_resource;
std::atomic<memory_resource*> default_resource{nullptr};

} // namespace

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

memory_resource& system_resource_instance() noexcept { return system_resource; }

memory_resource& resolve_resource(memory_resource* resource) noexcept {
  if (resource != nullptr) {
    return *resource;
  }

  memory_resource* current = default_resource.load(std::memory_order_acquire);
  return current != nullptr ? *current : system_resource;
}

memory_resource* replace_default_resource(memory_resource* resource) noexcept {
  memory_resource* old = default_resource.exchange(resource, std::memory_order_acq_rel);
  return old != nullptr ? old : &system_resource;
}

} // namespace caors::memory::detail
