// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_FIXED_BLOCK_POOL_H_
#define CAORS_MEMORY_FIXED_BLOCK_POOL_H_

#include "caors/memory/resource.h"

#include <cstddef>

namespace caors::memory {

/** @brief fixed_block_pool 的运行状态快照。 */
struct fixed_block_pool_statistics {
  std::size_t used_block_count = 0;
  std::size_t peak_used_block_count = 0;
  std::size_t total_block_count = 0;
  std::size_t free_block_count = 0;
  std::size_t chunk_count = 0;
  std::size_t allocation_count = 0;
  std::size_t deallocation_count = 0;
  std::size_t chunk_allocation_count = 0;
  std::size_t rejected_request_count = 0;
  std::size_t allocation_failure_count = 0;
  std::size_t release_count = 0;
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
  /** @brief 返回当前资源统计快照。 */
  [[nodiscard]] fixed_block_pool_statistics statistics() const noexcept;
  /** @brief 重置累计统计；当前水位和资源内容保持不变。 */
  void reset_statistics() noexcept;
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
  fixed_block_pool_statistics statistics_{};
};

} // namespace caors::memory

namespace caors {

using memory::fixed_block_pool;
using memory::fixed_block_pool_statistics;

} // namespace caors

#endif
