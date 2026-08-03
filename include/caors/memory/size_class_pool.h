// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_SIZE_CLASS_POOL_H_
#define CAORS_MEMORY_SIZE_CLASS_POOL_H_

#include "caors/memory/resource.h"

#include <cstddef>

namespace caors::memory {

/** @brief 分级内存池配置。所有尺寸与对齐均须为非零二次幂。 */
struct size_class_pool_options {
  std::size_t smallest_block_size = 16;
  std::size_t largest_block_size = 4096;
  /** @brief 单个 chunk 的最大块数。 */
  std::size_t blocks_per_chunk = 64;
  /** @brief 单个 size class 的目标 chunk 字节数；实际 chunk 至少包含一个块。 */
  std::size_t target_chunk_size = 4096;
  std::size_t pooled_alignment = default_memory_alignment;
};

/** @brief 分级内存池运行状态快照。 */
struct size_class_pool_statistics {
  std::size_t pooled_allocation_count = 0;
  std::size_t pooled_deallocation_count = 0;
  std::size_t fallback_allocation_count = 0;
  std::size_t fallback_deallocation_count = 0;
  std::size_t chunk_allocation_count = 0;
  std::size_t allocation_failure_count = 0;
  std::size_t chunk_count = 0;
  std::size_t pooled_requested_bytes_in_use = 0;
  std::size_t pooled_capacity_bytes_in_use = 0;
  std::size_t peak_pooled_capacity_bytes_in_use = 0;
  std::size_t reserved_bytes = 0;
  std::size_t release_count = 0;
};

/**
 * @brief 按二次幂尺寸等级复用中小型分配的非线程安全内存资源。
 *
 * 超过池化尺寸或对齐能力的请求直接转发给上游。调用方必须在释放时传回与分配时一致的大小和
 * 对齐；实例销毁或调用 release() 前不得仍有池内存活分配。本类型不拥有上游资源且不支持并发访问。
 */
class CAORS_API size_class_pool_resource final : public memory_resource {
public:
  /** @brief 使用默认配置创建资源；空上游解析为当前默认资源。 */
  explicit size_class_pool_resource(memory_resource* upstream = nullptr);
  /** @brief 使用指定配置创建资源；配置非法时抛出异常或触发 fail-fast。 */
  size_class_pool_resource(size_class_pool_options options, memory_resource* upstream = nullptr);
  ~size_class_pool_resource() override;

  size_class_pool_resource(const size_class_pool_resource&) = delete;
  size_class_pool_resource& operator=(const size_class_pool_resource&) = delete;

  /** @brief 返回非拥有的上游资源指针。 */
  [[nodiscard]] memory_resource* upstream_resource() const noexcept;
  /** @brief 返回构造时保存的不可变配置。 */
  [[nodiscard]] const size_class_pool_options& options() const noexcept;
  /** @brief 返回当前统计快照；本接口不提供并发访问保证。 */
  [[nodiscard]] size_class_pool_statistics statistics() const noexcept;
  /** @brief 重置累计计数，保留当前容量和使用水位。 */
  void reset_statistics() noexcept;
  /** @brief 释放全部池化 chunk；调用前不得仍有池内存活分配。 */
  void release() noexcept;

private:
  struct state;

  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override;
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override;
  [[nodiscard]] bool do_is_equal(const memory_resource& other) const noexcept override;

  state* state_ = nullptr;
};

} // namespace caors::memory

namespace caors {

using memory::size_class_pool_options;
using memory::size_class_pool_resource;
using memory::size_class_pool_statistics;

} // namespace caors

#endif
