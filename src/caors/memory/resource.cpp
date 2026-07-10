// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/resource.h"

#include "caors/config.h"
#include "detail/internal.h"

#include <new>

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

memory_block::~memory_block() { reset(); }

memory_block::memory_block(memory_block&& other) noexcept
    : ptr_(other.ptr_), size_(other.size_), alignment_(other.alignment_),
      resource_(other.resource_) {
  other.ptr_ = nullptr;
  other.size_ = 0;
  other.alignment_ = default_memory_alignment;
  other.resource_ = nullptr;
}

memory_block& memory_block::operator=(memory_block&& other) noexcept {
  if (this != &other) {
    reset();
    ptr_ = other.ptr_;
    size_ = other.size_;
    alignment_ = other.alignment_;
    resource_ = other.resource_;
    other.ptr_ = nullptr;
    other.size_ = 0;
    other.alignment_ = default_memory_alignment;
    other.resource_ = nullptr;
  }

  return *this;
}

void* memory_block::release() noexcept {
  void* ptr = ptr_;
  ptr_ = nullptr;
  return ptr;
}

void memory_block::reset() noexcept {
  if (ptr_ != nullptr && resource_ != nullptr) {
    resource_->deallocate(ptr_, size_, alignment_);
  }

  ptr_ = nullptr;
  size_ = 0;
  alignment_ = default_memory_alignment;
  resource_ = nullptr;
}

bool operator==(const memory_resource& lhs, const memory_resource& rhs) noexcept {
  return lhs.is_equal(rhs);
}

bool operator!=(const memory_resource& lhs, const memory_resource& rhs) noexcept {
  return !(lhs == rhs);
}

memory_resource& get_system_memory_resource() noexcept {
  return detail::system_resource_instance();
}

memory_resource* get_default_memory_resource() noexcept {
  return &detail::resolve_resource(nullptr);
}

memory_resource* set_default_memory_resource(memory_resource* resource) noexcept {
  return detail::replace_default_resource(resource);
}

memory_resource& library_resource() noexcept {
#if CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE
  return *get_default_memory_resource();
#else
  return get_system_memory_resource();
#endif
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

memory_block try_allocate_block(std::size_t size, std::size_t alignment,
                                memory_resource* resource) noexcept {
  if (!detail::is_power_of_two(alignment)) {
    return {};
  }

  memory_resource& resolved = detail::resolve_resource(resource);
  void* ptr = resolved.try_allocate(size, alignment);
  return ptr != nullptr ? memory_block{ptr, size, alignment, &resolved} : memory_block{};
}

memory_block allocate_block(std::size_t size, std::size_t alignment, memory_resource* resource) {
  detail::validate_alignment_or_fail(alignment);

  memory_resource& resolved = detail::resolve_resource(resource);
  void* ptr = resolved.allocate(size, alignment);
  return memory_block{ptr, size, alignment, &resolved};
}

} // namespace caors::memory
