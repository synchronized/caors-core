// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/tracking.h"

#include "memory_internal.h"

namespace caors::memory {

tracking_resource::tracking_resource(memory_resource* upstream) noexcept
    : upstream_(&detail::resolve_resource(upstream)) {}

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
  detail::update_peak(peak_bytes_in_use_, in_use);

  return ptr;
}

void tracking_resource::do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept {
  upstream_->deallocate(ptr, size, alignment);

  deallocation_count_.fetch_add(1, std::memory_order_relaxed);
  bytes_deallocated_.fetch_add(size, std::memory_order_relaxed);
  detail::subtract_saturating(bytes_in_use_, size);
}

bool tracking_resource::do_is_equal(const memory_resource&) const noexcept { return false; }

} // namespace caors::memory
