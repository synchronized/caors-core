// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_RESOURCE_H_
#define CAORS_MEMORY_RESOURCE_H_

#include "caors/config.h"
#include "caors/export.h"

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace caors::memory {

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

class CAORS_API system_memory_resource final : public memory_resource {
private:
  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override;
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override;
  bool do_is_equal(const memory_resource& other) const noexcept override;
};

class CAORS_API callback_memory_resource final : public memory_resource {
public:
  using allocate_fn = void* (*)(std::size_t size, std::size_t alignment, void* user_data);
  using deallocate_fn = void (*)(void* ptr, std::size_t size, std::size_t alignment,
                                 void* user_data);

  callback_memory_resource(allocate_fn allocate, deallocate_fn deallocate,
                           void* user_data = nullptr);

  [[nodiscard]] allocate_fn allocate_callback() const noexcept;
  [[nodiscard]] deallocate_fn deallocate_callback() const noexcept;
  [[nodiscard]] void* user_data() const noexcept;

private:
  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override;
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override;
  bool do_is_equal(const memory_resource& other) const noexcept override;

  allocate_fn allocate_fn_ = nullptr;
  deallocate_fn deallocate_fn_ = nullptr;
  void* user_data_ = nullptr;
};

/** @brief 拥有一块裸内存的 RAII 句柄。 */
class CAORS_API memory_block final {
public:
  constexpr memory_block() noexcept = default;
  ~memory_block();

  memory_block(const memory_block&) = delete;
  memory_block& operator=(const memory_block&) = delete;

  memory_block(memory_block&& other) noexcept;
  memory_block& operator=(memory_block&& other) noexcept;

  /** @brief 返回裸内存起始地址。 */
  [[nodiscard]] void* data() const noexcept { return ptr_; }
  /** @brief 返回分配时记录的大小。 */
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  /** @brief 返回分配时记录的对齐。 */
  [[nodiscard]] std::size_t alignment() const noexcept { return alignment_; }
  /** @brief 返回释放时使用的资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }
  /** @brief 判断当前是否拥有内存。 */
  [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }

  /**
   * @brief 拆出当前裸指针并放弃所有权。
   *
   * 调用方接管释放责任，必须使用返回前记录的资源、大小和对齐释放该指针。
   */
  [[nodiscard]] void* detach() noexcept;
  /** @brief 释放当前内存并清空句柄。 */
  void reset() noexcept;

private:
  friend memory_block try_allocate_block(std::size_t size, std::size_t alignment,
                                         memory_resource* resource) noexcept;
  friend memory_block allocate_block(std::size_t size, std::size_t alignment,
                                     memory_resource* resource);

  constexpr memory_block(void* ptr, std::size_t size, std::size_t alignment,
                         memory_resource* resource) noexcept
      : ptr_(ptr), size_(size), alignment_(alignment), resource_(resource) {}

  void* ptr_ = nullptr;
  std::size_t size_ = 0;
  std::size_t alignment_ = default_memory_alignment;
  memory_resource* resource_ = nullptr;
};

[[nodiscard]] CAORS_API bool operator==(const memory_resource& lhs,
                                        const memory_resource& rhs) noexcept;
[[nodiscard]] CAORS_API bool operator!=(const memory_resource& lhs,
                                        const memory_resource& rhs) noexcept;

[[nodiscard]] CAORS_API memory_resource& get_system_memory_resource() noexcept;
[[nodiscard]] CAORS_API memory_resource* get_default_memory_resource() noexcept;
CAORS_API memory_resource* set_default_memory_resource(memory_resource* resource) noexcept;
/**
 * @brief CAORS 库内部全局元数据使用的内存资源。
 *
 * 默认固定使用 system resource，避免静态初始化阶段尚未设置默认资源时，类型注册、
 * `string_name` 等全局表捕获到不可预期资源。需要调试内部元数据分配时，可以在编译期定义
 * `CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE=1`，让内部表跟随当前默认资源。
 */
[[nodiscard]] CAORS_API memory_resource& library_resource() noexcept;

[[nodiscard]] CAORS_API void* try_allocate(std::size_t size,
                                           std::size_t alignment = default_memory_alignment,
                                           memory_resource* resource = nullptr) noexcept;
[[nodiscard]] CAORS_API void* allocate(std::size_t size,
                                       std::size_t alignment = default_memory_alignment,
                                       memory_resource* resource = nullptr);
CAORS_API void deallocate(void* ptr, std::size_t size,
                          std::size_t alignment = default_memory_alignment,
                          memory_resource* resource = nullptr) noexcept;
[[nodiscard]] CAORS_API memory_block
try_allocate_block(std::size_t size, std::size_t alignment = default_memory_alignment,
                   memory_resource* resource = nullptr) noexcept;
[[nodiscard]] CAORS_API memory_block
allocate_block(std::size_t size, std::size_t alignment = default_memory_alignment,
               memory_resource* resource = nullptr);

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

  template <typename U, typename... Args> void construct(U* ptr, Args&&... args) {
    std::uninitialized_construct_using_allocator(ptr, allocator<U>{*this},
                                                 std::forward<Args>(args)...);
  }

  template <typename U> void destroy(U* ptr) noexcept { std::destroy_at(ptr); }

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
    auto* value = static_cast<T*>(storage);
    allocator<T>{resolved}.construct(value, std::forward<Args>(args)...);
    return value;
  } catch (...) {
    resolved->deallocate(storage, sizeof(T), alignof(T));
    throw;
  }
#else
  auto* value = static_cast<T*>(storage);
  allocator<T>{resolved}.construct(value, std::forward<Args>(args)...);
  return value;
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

} // namespace caors::memory

namespace caors {

using memory::deallocate;
using memory::default_memory_alignment;
using memory::destroy;
using memory::get_default_memory_resource;
using memory::get_system_memory_resource;
using memory::library_resource;
using memory::make;
using memory::memory_block;
using memory::memory_resource;
using memory::operator!=;
using memory::operator==;
using memory::allocate;
using memory::allocate_block;
using memory::set_default_memory_resource;
using memory::system_memory_resource;
using memory::try_allocate;
using memory::try_allocate_block;

template <typename T> using allocator = memory::allocator<T>;

} // namespace caors

#endif
