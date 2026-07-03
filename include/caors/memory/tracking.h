// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_TRACKING_H_
#define CAORS_MEMORY_TRACKING_H_

#include "caors/memory/resource.h"

#include <atomic>
#include <cstddef>

namespace caors::memory {

struct memory_statistics {
  std::size_t allocation_count = 0;
  std::size_t deallocation_count = 0;
  std::size_t bytes_allocated = 0;
  std::size_t bytes_deallocated = 0;
  std::size_t bytes_in_use = 0;
  std::size_t peak_bytes_in_use = 0;
};

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

} // namespace caors::memory

namespace caors {

using memory::memory_statistics;
using memory::tracking_resource;

} // namespace caors

#endif
