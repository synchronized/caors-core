// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_RESOURCE_H_
#define CAORS_MEMORY_RESOURCE_H_

#include "caors/config.h"
#include "caors/export.h"

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace caors {

inline constexpr std::size_t default_memory_alignment = alignof(std::max_align_t);

class CAORS_API memory_resource {
public:
  virtual ~memory_resource() = default;

  [[nodiscard]] void* try_allocate(std::size_t size,
                                   std::size_t alignment = default_memory_alignment) noexcept;
  [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment = default_memory_alignment);
  void deallocate(void* ptr, std::size_t size,
                  std::size_t alignment = default_memory_alignment) noexcept;

  [[nodiscard]] bool is_equal(const memory_resource& other) const noexcept;

private:
  virtual void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept = 0;
  virtual void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept = 0;
  virtual bool do_is_equal(const memory_resource& other) const noexcept = 0;
};

using memory_allocate_fn = void* (*)(std::size_t size, std::size_t alignment, void* user_data);
using memory_deallocate_fn = void (*)(void* ptr, std::size_t size, std::size_t alignment,
                                      void* user_data);

class CAORS_API system_memory_resource final : public memory_resource {
private:
  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override;
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override;
  bool do_is_equal(const memory_resource& other) const noexcept override;
};

class CAORS_API callback_memory_resource final : public memory_resource {
public:
  callback_memory_resource(memory_allocate_fn allocate_fn, memory_deallocate_fn deallocate_fn,
                           void* user_data = nullptr);

  [[nodiscard]] memory_allocate_fn allocate_callback() const noexcept;
  [[nodiscard]] memory_deallocate_fn deallocate_callback() const noexcept;
  [[nodiscard]] void* user_data() const noexcept;

private:
  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override;
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override;
  bool do_is_equal(const memory_resource& other) const noexcept override;

  memory_allocate_fn allocate_fn_ = nullptr;
  memory_deallocate_fn deallocate_fn_ = nullptr;
  void* user_data_ = nullptr;
};

[[nodiscard]] CAORS_API bool operator==(const memory_resource& lhs,
                                        const memory_resource& rhs) noexcept;
[[nodiscard]] CAORS_API bool operator!=(const memory_resource& lhs,
                                        const memory_resource& rhs) noexcept;

[[nodiscard]] CAORS_API memory_resource& get_system_memory_resource() noexcept;
[[nodiscard]] CAORS_API memory_resource* get_default_memory_resource() noexcept;
CAORS_API memory_resource* set_default_memory_resource(memory_resource* resource) noexcept;

[[nodiscard]] CAORS_API void* try_allocate(std::size_t size,
                                           std::size_t alignment = default_memory_alignment,
                                           memory_resource* resource = nullptr) noexcept;
[[nodiscard]] CAORS_API void* allocate(std::size_t size,
                                       std::size_t alignment = default_memory_alignment,
                                       memory_resource* resource = nullptr);
CAORS_API void deallocate(void* ptr, std::size_t size,
                          std::size_t alignment = default_memory_alignment,
                          memory_resource* resource = nullptr) noexcept;

template <typename T> class allocator {
public:
  using value_type = T;
  using propagate_on_container_move_assignment = std::true_type;
  using is_always_equal = std::false_type;

  allocator() noexcept : resource_(get_default_memory_resource()) {}

  explicit allocator(memory_resource* resource) noexcept
      : resource_(resource != nullptr ? resource : get_default_memory_resource()) {}

  template <typename U>
  allocator(const allocator<U>& other) noexcept : resource_(other.resource()) {}

  [[nodiscard]] T* allocate(std::size_t count) {
    if (count > max_size()) {
#if CAORS_HAS_EXCEPTIONS
      throw std::bad_array_new_length();
#else
      std::abort();
#endif
    }

    return static_cast<T*>(resource_->allocate(sizeof(T) * count, alignof(T)));
  }

  void deallocate(T* ptr, std::size_t count) noexcept {
    resource_->deallocate(ptr, sizeof(T) * count, alignof(T));
  }

  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }
  [[nodiscard]] static constexpr std::size_t max_size() noexcept {
    return std::numeric_limits<std::size_t>::max() / sizeof(T);
  }

private:
  template <typename> friend class allocator;

  memory_resource* resource_ = nullptr;
};

template <typename T, typename U>
[[nodiscard]] bool operator==(const allocator<T>& lhs, const allocator<U>& rhs) noexcept {
  return *lhs.resource() == *rhs.resource();
}

template <typename T, typename U>
[[nodiscard]] bool operator!=(const allocator<T>& lhs, const allocator<U>& rhs) noexcept {
  return !(lhs == rhs);
}

template <typename T, typename... Args>
[[nodiscard]] T* make(memory_resource* resource, Args&&... args) {
  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  void* storage = resolved->allocate(sizeof(T), alignof(T));

#if CAORS_HAS_EXCEPTIONS
  try {
    return ::new (storage) T(std::forward<Args>(args)...);
  } catch (...) {
    resolved->deallocate(storage, sizeof(T), alignof(T));
    throw;
  }
#else
  return ::new (storage) T(std::forward<Args>(args)...);
#endif
}

template <typename T> [[nodiscard]] T* make() {
  return make<T>(static_cast<memory_resource*>(nullptr));
}

template <typename T, typename First, typename... Rest>
  requires(!std::is_convertible_v<std::remove_cvref_t<First>, memory_resource*>)
[[nodiscard]] T* make(First&& first, Rest&&... rest) {
  return make<T>(static_cast<memory_resource*>(nullptr), std::forward<First>(first),
                 std::forward<Rest>(rest)...);
}

template <typename T> void destroy(T* ptr, memory_resource* resource = nullptr) noexcept {
  if (ptr == nullptr) {
    return;
  }

  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  ptr->~T();
  resolved->deallocate(ptr, sizeof(T), alignof(T));
}

} // namespace caors

#endif
