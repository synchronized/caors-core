// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_STACK_H_
#define CAORS_MEMORY_STACK_H_

#include "caors/memory/resource.h"

#include <cstddef>
#include <cstdint>

namespace caors::memory {

/** @brief stack_resource 的运行状态快照。 */
struct stack_resource_statistics {
  std::size_t bytes_in_use = 0;
  std::size_t peak_bytes_in_use = 0;
  std::size_t chunk_count = 0;
  std::size_t peak_chunk_count = 0;
  std::size_t large_allocation_count = 0;
  std::size_t mark_count = 0;
  std::size_t rewind_count = 0;
  std::size_t trim_count = 0;
  std::size_t reset_count = 0;
};

/**
 * @brief 支持作用域标记与批量回滚的单调内存资源。
 *
 * 该资源不是线程安全的。标记、分配和回滚必须发生在同一线程，并严格遵循后进先出顺序。
 * 回滚只释放存储，不调用放置在资源中的对象析构函数。
 */
class CAORS_API stack_resource final : public memory_resource {
public:
  /** @brief 表示资源中一个可回滚的位置。 */
  class marker final {
  public:
    constexpr marker() noexcept = default;

  private:
    friend class stack_resource;

    constexpr marker(const stack_resource* owner, void* chunk, std::size_t offset,
                     std::size_t bytes_in_use, std::size_t depth, std::uint64_t generation,
                     std::uint64_t id, std::uint64_t parent_id) noexcept
        : owner_(owner), chunk_(chunk), offset_(offset), depth_(depth), generation_(generation),
          id_(id), parent_id_(parent_id), bytes_in_use_(bytes_in_use) {}

    const stack_resource* owner_ = nullptr;
    void* chunk_ = nullptr;
    std::size_t offset_ = 0;
    std::size_t depth_ = 0;
    std::uint64_t generation_ = 0;
    std::uint64_t id_ = 0;
    std::uint64_t parent_id_ = 0;
    std::size_t bytes_in_use_ = 0;
  };

  explicit stack_resource(std::size_t initial_capacity = 64 * 1024,
                          memory_resource* upstream = nullptr);
  ~stack_resource() override;

  stack_resource(const stack_resource&) = delete;
  stack_resource& operator=(const stack_resource&) = delete;

  /** @brief 返回上游资源。 */
  [[nodiscard]] memory_resource* upstream_resource() const noexcept;
  /** @brief 返回基础 chunk 容量。 */
  [[nodiscard]] std::size_t initial_capacity() const noexcept;
  /** @brief 创建一个必须按后进先出顺序回滚的标记。 */
  [[nodiscard]] marker mark() noexcept;
  /** @brief 回滚到指定标记；无效或乱序标记在 Debug 构建中触发断言。 */
  void rewind(marker position) noexcept;
  /** @brief 释放全部 chunk；仅允许在没有活动标记时调用。 */
  void reset() noexcept;
  /** @brief 释放基础 chunk 之外的缓存；仅允许在没有活动标记时调用。 */
  void trim() noexcept;
  /** @brief 返回当前活动标记数量。 */
  [[nodiscard]] std::size_t active_scope_count() const noexcept;
  /** @brief 返回当前资源统计快照。 */
  [[nodiscard]] stack_resource_statistics statistics() const noexcept;
  /** @brief 重置累计统计；当前水位和资源内容保持不变。 */
  void reset_statistics() noexcept;

private:
  struct chunk;

  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override;
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override;
  bool do_is_equal(const memory_resource& other) const noexcept override;

  [[nodiscard]] bool try_add_chunk(std::size_t capacity, bool large) noexcept;
  [[nodiscard]] void* try_allocate_from(chunk& target, std::size_t size,
                                        std::size_t alignment) noexcept;
  void release_chunk(chunk* target) noexcept;
  void release_all() noexcept;

  memory_resource* upstream_ = nullptr;
  chunk* chunks_ = nullptr;
  chunk* tail_ = nullptr;
  chunk* current_ = nullptr;
  std::size_t initial_capacity_ = 0;
  std::size_t next_capacity_ = 0;
  std::size_t active_depth_ = 0;
  std::uint64_t generation_ = 1;
  std::uint64_t marker_sequence_ = 0;
  std::uint64_t active_marker_id_ = 0;
  stack_resource_statistics statistics_{};
};

/** @brief 在析构时将 stack_resource 回滚到构造前位置。 */
class CAORS_API stack_scope final {
public:
  explicit stack_scope(stack_resource& resource) noexcept;
  ~stack_scope();

  stack_scope(const stack_scope&) = delete;
  stack_scope& operator=(const stack_scope&) = delete;
  stack_scope(stack_scope&&) = delete;
  stack_scope& operator=(stack_scope&&) = delete;

  /** @brief 返回本作用域使用的资源。 */
  [[nodiscard]] memory_resource* resource() noexcept;

private:
  stack_resource* resource_ = nullptr;
  stack_resource::marker marker_{};
};

/**
 * @brief 返回当前线程复用的 CAORS 临时内存资源。
 *
 * 返回值在线程退出前有效。跨动态库边界时，优先由 host 显式传递资源。
 */
[[nodiscard]] CAORS_API stack_resource& library_temporary_resource() noexcept;

} // namespace caors::memory

namespace caors {

using memory::library_temporary_resource;
using memory::stack_resource;
using memory::stack_resource_statistics;
using memory::stack_scope;

} // namespace caors

#endif
