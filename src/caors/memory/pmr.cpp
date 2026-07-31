// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/pmr.h"

#include "caors/config.h"

namespace caors::memory {

pmr_resource_view::pmr_resource_view(caors::memory::memory_resource& resource) noexcept
    : resource_(&resource) {}

caors::memory::memory_resource& pmr_resource_view::resource() const noexcept { return *resource_; }

void* pmr_resource_view::do_allocate(std::size_t size, std::size_t alignment) {
  return resource_->allocate(size, alignment);
}

void pmr_resource_view::do_deallocate(void* ptr, std::size_t size, std::size_t alignment) {
  resource_->deallocate(ptr, size, alignment);
}

bool pmr_resource_view::do_is_equal(const std::pmr::memory_resource& other) const noexcept {
  const auto* rhs = dynamic_cast<const pmr_resource_view*>(&other);
  return rhs != nullptr && resource_->is_equal(*rhs->resource_);
}

caors_resource_view::caors_resource_view(std::pmr::memory_resource& resource) noexcept
    : resource_(&resource) {}

std::pmr::memory_resource& caors_resource_view::resource() const noexcept { return *resource_; }

void* caors_resource_view::do_try_allocate(std::size_t size, std::size_t alignment) noexcept {
#if CAORS_HAS_EXCEPTIONS
  try {
    return resource_->allocate(size, alignment);
  } catch (...) {
    return nullptr;
  }
#else
  return resource_->allocate(size, alignment);
#endif
}

void caors_resource_view::do_deallocate(void* ptr, std::size_t size,
                                        std::size_t alignment) noexcept {
  resource_->deallocate(ptr, size, alignment);
}

bool caors_resource_view::do_is_equal(const memory_resource& other) const noexcept {
  const auto* rhs = dynamic_cast<const caors_resource_view*>(&other);
  return rhs != nullptr && resource_->is_equal(*rhs->resource_);
}

} // namespace caors::memory
