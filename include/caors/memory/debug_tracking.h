// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_DEBUG_TRACKING_H_
#define CAORS_MEMORY_DEBUG_TRACKING_H_

#include "caors/memory/tracking.h"

#include <cstddef>
#include <cstdint>
#include <source_location>

namespace caors::memory {

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
  std::size_t front_guard_corruption_count = 0;
  std::size_t back_guard_corruption_count = 0;
};

/** @brief 调试资源的 guard 与内存填充选项。 */
struct memory_debug_options {
  std::size_t guard_size = 16;
  std::byte front_guard_pattern{0xF1};
  std::byte back_guard_pattern{0xF2};
  std::byte allocation_fill_pattern{0xCD};
  std::byte deallocation_fill_pattern{0xDD};
  bool fill_on_allocate = true;
  bool fill_on_deallocate = true;
};

/** @brief 调试资源检测到的违规类型。 */
enum class memory_debug_violation_kind {
  invalid_deallocation,
  mismatched_deallocation,
  front_guard_corruption,
  back_guard_corruption,
};

/** @brief 单次内存违规的诊断快照。 */
struct memory_debug_violation {
  memory_debug_violation_kind kind = memory_debug_violation_kind::invalid_deallocation;
  memory_debug_record allocation;
  std::size_t supplied_size = 0;
  std::size_t supplied_alignment = 0;
};

struct debug_tracking_state;

/**
 * @brief 记录存活分配和释放错误的调试包装资源。
 *
 * 该资源维护存活分配表，适合开发期、测试期和泄漏排查；不建议默认放入性能敏感热路径。
 */
class CAORS_API debug_tracking_resource final : public memory_resource {
public:
  using record_fn = void (*)(const memory_debug_record& record, void* user_data);
  using violation_fn = void (*)(const memory_debug_violation& violation, void* user_data) noexcept;

  explicit debug_tracking_resource(memory_resource* upstream = nullptr);
  debug_tracking_resource(memory_resource* upstream, memory_debug_options options);
  ~debug_tracking_resource() override;

  debug_tracking_resource(const debug_tracking_resource&) = delete;
  debug_tracking_resource& operator=(const debug_tracking_resource&) = delete;

  [[nodiscard]] memory_resource* upstream_resource() const noexcept;
  [[nodiscard]] memory_debug_statistics debug_statistics() const noexcept;
  [[nodiscard]] std::size_t live_allocation_count() const noexcept;
  [[nodiscard]] std::size_t live_bytes() const noexcept;
  [[nodiscard]] bool has_leaks() const noexcept;
  [[nodiscard]] memory_debug_options options() const noexcept;

  /**
   * @brief 设置非拥有违规诊断回调。
   *
   * 回调在内部记录锁外执行。调用方必须保证 `user_data` 在可能发生的回调期间有效。
   */
  void set_violation_callback(violation_fn callback, void* user_data = nullptr) noexcept;

  /**
   * @brief 遍历当前存活分配记录。
   *
   * 该入口不返回容器，避免 memory 层依赖 core 容器层。回调会在内部调试记录锁保护期间被调用，
   * 回调内不应再次调用同一个资源的会获取调试记录锁的接口。
   */
  void for_each_live_allocation(record_fn callback, void* user_data = nullptr) const noexcept;

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

} // namespace caors::memory

namespace caors {} // namespace caors

#endif
