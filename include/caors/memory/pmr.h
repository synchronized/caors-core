// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_PMR_H_
#define CAORS_MEMORY_PMR_H_

#include "caors/export.h"
#include "caors/memory/resource.h"

#include <cstddef>
#include <memory_resource>

namespace caors::memory {

/**
 * @brief 将 CAORS 内存资源暴露为标准 PMR 资源的非拥有视图。
 *
 * 视图和底层 CAORS 资源必须比所有引用该视图的 PMR allocator、容器及分配结果活得更久。
 * 该类型不增加同步，也不会修改 `std::pmr` 默认资源。
 */
class CAORS_API pmr_resource_view final : public std::pmr::memory_resource {
public:
  explicit pmr_resource_view(caors::memory::memory_resource& resource) noexcept;

  /** @brief 返回底层 CAORS 内存资源。 */
  [[nodiscard]] caors::memory::memory_resource& resource() const noexcept;

private:
  void* do_allocate(std::size_t size, std::size_t alignment) override;
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) override;
  [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;

  caors::memory::memory_resource* resource_ = nullptr;
};

/**
 * @brief 将标准 PMR 资源暴露为 CAORS 内存资源的非拥有视图。
 *
 * 视图和底层 PMR 资源必须比所有引用该视图的 CAORS allocator、容器及分配结果活得更久。
 * 开启异常时，底层 PMR 分配抛出的异常会在 `try_allocate()` 路径转换为空指针。
 * 该类型不增加同步，也不会修改 CAORS 默认资源。
 */
class CAORS_API caors_resource_view final : public memory_resource {
public:
  explicit caors_resource_view(std::pmr::memory_resource& resource) noexcept;

  /** @brief 返回底层标准 PMR 资源。 */
  [[nodiscard]] std::pmr::memory_resource& resource() const noexcept;

private:
  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override;
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override;
  [[nodiscard]] bool do_is_equal(const memory_resource& other) const noexcept override;

  std::pmr::memory_resource* resource_ = nullptr;
};

} // namespace caors::memory

namespace caors {

using memory::caors_resource_view;
using memory::pmr_resource_view;

} // namespace caors

#endif
