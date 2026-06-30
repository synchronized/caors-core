// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_H_
#define CAORS_MEMORY_H_

#include "caors/config.h"
#include "caors/export.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>
#include <source_location>
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

struct memory_statistics {
  std::size_t allocation_count = 0;
  std::size_t deallocation_count = 0;
  std::size_t bytes_allocated = 0;
  std::size_t bytes_deallocated = 0;
  std::size_t bytes_in_use = 0;
  std::size_t peak_bytes_in_use = 0;
};

/**
 * @brief 单个存活分配的调试记录。
 *
 * `label` 由调用方管理生命周期；通过 `allocate_debug()` 捕获时，文件名、函数名、
 * 行号和列号来自 `std::source_location`。
 */
struct memory_debug_record {
  void* ptr = nullptr;
  std::size_t size = 0;
  std::size_t alignment = 0;
  const char* label = nullptr;
  const char* file_name = nullptr;
  const char* function_name = nullptr;
  std::uint_least32_t line = 0;
  std::uint_least32_t column = 0;
};

/** @brief 调试分配器的统计快照。 */
struct memory_debug_statistics {
  memory_statistics memory;
  std::size_t live_allocation_count = 0;
  std::size_t invalid_deallocation_count = 0;
  std::size_t mismatched_deallocation_count = 0;
};

using memory_debug_record_fn = void (*)(const memory_debug_record& record, void* user_data);

class CAORS_API tracking_resource final : public memory_resource {
public:
  explicit tracking_resource(memory_resource* upstream = nullptr) noexcept;

  [[nodiscard]] memory_resource* upstream_resource() const noexcept;
  [[nodiscard]] memory_statistics statistics() const noexcept;
  void reset_statistics() noexcept;

private:
  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override;
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override;
  bool do_is_equal(const memory_resource& other) const noexcept override;

  memory_resource* upstream_ = nullptr;
  std::atomic<std::size_t> allocation_count_{0};
  std::atomic<std::size_t> deallocation_count_{0};
  std::atomic<std::size_t> bytes_allocated_{0};
  std::atomic<std::size_t> bytes_deallocated_{0};
  std::atomic<std::size_t> bytes_in_use_{0};
  std::atomic<std::size_t> peak_bytes_in_use_{0};
};

struct debug_tracking_state;

/**
 * @brief 记录存活分配和释放错误的调试包装资源。
 *
 * 该资源维护存活分配表，适合开发期、测试期和泄漏排查；不建议默认放入性能敏感热路径。
 */
class CAORS_API debug_tracking_resource final : public memory_resource {
public:
  explicit debug_tracking_resource(memory_resource* upstream = nullptr);
  ~debug_tracking_resource() override;

  debug_tracking_resource(const debug_tracking_resource&) = delete;
  debug_tracking_resource& operator=(const debug_tracking_resource&) = delete;

  [[nodiscard]] memory_resource* upstream_resource() const noexcept;
  [[nodiscard]] memory_debug_statistics debug_statistics() const noexcept;
  [[nodiscard]] std::size_t live_allocation_count() const noexcept;
  [[nodiscard]] std::size_t live_bytes() const noexcept;
  [[nodiscard]] bool has_leaks() const noexcept;

  /**
   * @brief 遍历当前存活分配记录。
   *
   * 该入口不返回容器，避免 memory 层依赖 core 容器层。回调会在内部调试记录锁保护期间被调用，
   * 回调内不应再次调用同一个资源的会获取调试记录锁的接口。
   */
  void for_each_live_allocation(memory_debug_record_fn callback,
                                void* user_data = nullptr) const noexcept;

  /** @brief 分配内存并记录标签和调用位置，失败返回 `nullptr`。 */
  [[nodiscard]] void*
  try_allocate_debug(std::size_t size, std::size_t alignment = default_memory_alignment,
                     const char* label = nullptr,
                     std::source_location location = std::source_location::current()) noexcept;
  /** @brief 使用默认对齐分配内存并记录标签和调用位置，失败返回 `nullptr`。 */
  [[nodiscard]] void*
  try_allocate_debug(std::size_t size, const char* label,
                     std::source_location location = std::source_location::current()) noexcept;

  /** @brief 分配内存并记录标签和调用位置，失败时按 `allocate()` 语义处理。 */
  [[nodiscard]] void*
  allocate_debug(std::size_t size, std::size_t alignment = default_memory_alignment,
                 const char* label = nullptr,
                 std::source_location location = std::source_location::current());
  /** @brief 使用默认对齐分配内存并记录标签和调用位置，失败时按 `allocate()` 语义处理。 */
  [[nodiscard]] void*
  allocate_debug(std::size_t size, const char* label,
                 std::source_location location = std::source_location::current());

private:
  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override;
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override;
  bool do_is_equal(const memory_resource& other) const noexcept override;

  [[nodiscard]] void* try_allocate_recorded(std::size_t size, std::size_t alignment,
                                            const char* label,
                                            std::source_location location) noexcept;

  debug_tracking_state* state_ = nullptr;
};

class CAORS_API monotonic_resource final : public memory_resource {
public:
  explicit monotonic_resource(std::size_t initial_capacity = 4096,
                              memory_resource* upstream = nullptr);
  ~monotonic_resource() override;

  monotonic_resource(const monotonic_resource&) = delete;
  monotonic_resource& operator=(const monotonic_resource&) = delete;

  [[nodiscard]] memory_resource* upstream_resource() const noexcept;
  [[nodiscard]] std::size_t initial_capacity() const noexcept;
  void reset() noexcept;

private:
  struct chunk;

  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override;
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override;
  bool do_is_equal(const memory_resource& other) const noexcept override;

  [[nodiscard]] bool try_add_chunk(std::size_t capacity) noexcept;

  memory_resource* upstream_ = nullptr;
  chunk* chunks_ = nullptr;
  std::size_t initial_capacity_ = 0;
  std::size_t next_capacity_ = 0;
};

class CAORS_API fixed_block_pool final : public memory_resource {
public:
  fixed_block_pool(std::size_t block_size, std::size_t block_alignment = default_memory_alignment,
                   std::size_t blocks_per_chunk = 64, memory_resource* upstream = nullptr);
  ~fixed_block_pool() override;

  fixed_block_pool(const fixed_block_pool&) = delete;
  fixed_block_pool& operator=(const fixed_block_pool&) = delete;

  [[nodiscard]] memory_resource* upstream_resource() const noexcept;
  [[nodiscard]] std::size_t block_size() const noexcept;
  [[nodiscard]] std::size_t block_alignment() const noexcept;
  [[nodiscard]] std::size_t blocks_per_chunk() const noexcept;
  void release() noexcept;

private:
  struct chunk;
  struct free_block;

  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override;
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override;
  bool do_is_equal(const memory_resource& other) const noexcept override;

  [[nodiscard]] bool try_add_chunk() noexcept;

  memory_resource* upstream_ = nullptr;
  chunk* chunks_ = nullptr;
  free_block* free_blocks_ = nullptr;
  std::size_t block_size_ = 0;
  std::size_t block_alignment_ = 0;
  std::size_t blocks_per_chunk_ = 0;
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

}

#endif
