// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_MONOTONIC_H_
#define CAORS_MEMORY_MONOTONIC_H_

#include "caors/memory/resource.h"

#include <cstddef>

namespace caors::memory {

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

} // namespace caors::memory

namespace caors {

using memory::monotonic_resource;

} // namespace caors

#endif
