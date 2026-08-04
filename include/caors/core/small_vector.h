// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_CORE_SMALL_VECTOR_H_
#define CAORS_CORE_SMALL_VECTOR_H_

#include "caors/memory/resource.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace caors::core {

/**
 * @brief 带固定内联容量的连续顺序容器。
 *
 * 元素数量不超过 `InlineCapacity` 时不发生动态分配，超过后使用指定 CAORS resource 保存连续
 * 动态存储。`resource` 非拥有，必须比容器及其动态存储存活更久。
 *
 * @tparam T 元素类型。
 * @tparam InlineCapacity 对象内部可容纳的元素数量，可以为零。
 */
template <typename T, std::size_t InlineCapacity> class small_vector {
public:
  using value_type = T;
  using allocator_type = memory::allocator<T>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  static_assert(InlineCapacity <= allocator_type::max_size(),
                "small_vector 内联容量超过元素数量上限");
  static_assert(InlineCapacity <=
                    static_cast<size_type>((std::numeric_limits<difference_type>::max)()),
                "small_vector 内联容量超过迭代器差值上限");

  /** @brief 使用当前默认内存资源构造空容器。 */
  small_vector() noexcept : small_vector(nullptr) {}

  /**
   * @brief 使用指定资源构造空容器。
   * @param resource 动态存储使用的非拥有资源；为空时捕获当前默认资源。
   */
  explicit small_vector(memory::memory_resource* resource) noexcept
      : data_(inline_data()), resource_(resolve_resource(resource)) {}

  small_vector(const small_vector&) = delete;
  small_vector& operator=(const small_vector&) = delete;
  small_vector(small_vector&&) = delete;
  small_vector& operator=(small_vector&&) = delete;

  /** @brief 销毁已构造元素并释放动态存储。 */
  ~small_vector() {
    std::destroy_n(data_, size_);
    if (!uses_inline_storage()) {
      allocator_type{resource_}.deallocate(data_, capacity_);
    }
  }

  /** @brief 返回容器捕获的内存资源。 */
  [[nodiscard]] memory::memory_resource* resource() const noexcept { return resource_; }

  /** @brief 返回绑定到容器资源的 allocator。 */
  [[nodiscard]] allocator_type get_allocator() const noexcept { return allocator_type{resource_}; }

  /** @brief 返回编译期内联容量。 */
  [[nodiscard]] static constexpr size_type inline_capacity() noexcept { return InlineCapacity; }

  /** @brief 返回当前元素数量。 */
  [[nodiscard]] size_type size() const noexcept { return size_; }

  /** @brief 返回当前可容纳元素数量。 */
  [[nodiscard]] size_type capacity() const noexcept { return capacity_; }

  /** @brief 返回容器是否为空。 */
  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

  /** @brief 返回理论最大元素数量。 */
  [[nodiscard]] static constexpr size_type max_size() noexcept {
    constexpr size_type difference_limit =
        static_cast<size_type>((std::numeric_limits<difference_type>::max)());
    return (std::min)(allocator_type::max_size(), difference_limit);
  }

  /** @brief 返回当前是否使用对象内联存储。 */
  [[nodiscard]] bool uses_inline_storage() const noexcept { return data_ == inline_data(); }

  /** @brief 返回连续元素存储的起始地址。 */
  [[nodiscard]] pointer data() noexcept { return data_; }
  /** @brief 返回连续元素存储的只读起始地址。 */
  [[nodiscard]] const_pointer data() const noexcept { return data_; }

  /** @brief 返回正向迭代起点。 */
  [[nodiscard]] iterator begin() noexcept { return data_; }
  /** @brief 返回只读正向迭代起点。 */
  [[nodiscard]] const_iterator begin() const noexcept { return data_; }
  /** @brief 返回只读正向迭代起点。 */
  [[nodiscard]] const_iterator cbegin() const noexcept { return data_; }
  /** @brief 返回正向迭代终点。 */
  [[nodiscard]] iterator end() noexcept { return data_ + size_; }
  /** @brief 返回只读正向迭代终点。 */
  [[nodiscard]] const_iterator end() const noexcept { return data_ + size_; }
  /** @brief 返回只读正向迭代终点。 */
  [[nodiscard]] const_iterator cend() const noexcept { return data_ + size_; }
  /** @brief 返回反向迭代起点。 */
  [[nodiscard]] reverse_iterator rbegin() noexcept { return reverse_iterator{end()}; }
  /** @brief 返回只读反向迭代起点。 */
  [[nodiscard]] const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator{end()};
  }
  /** @brief 返回只读反向迭代起点。 */
  [[nodiscard]] const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator{cend()};
  }
  /** @brief 返回反向迭代终点。 */
  [[nodiscard]] reverse_iterator rend() noexcept { return reverse_iterator{begin()}; }
  /** @brief 返回只读反向迭代终点。 */
  [[nodiscard]] const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator{begin()};
  }
  /** @brief 返回只读反向迭代终点。 */
  [[nodiscard]] const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator{cbegin()};
  }

  /** @brief 不检查边界地访问元素。 */
  [[nodiscard]] reference operator[](size_type index) noexcept { return data_[index]; }
  /** @brief 不检查边界地只读访问元素。 */
  [[nodiscard]] const_reference operator[](size_type index) const noexcept { return data_[index]; }

  /** @brief 检查边界地访问元素。 */
  [[nodiscard]] reference at(size_type index) {
    check_index(index);
    return data_[index];
  }
  /** @brief 检查边界地只读访问元素。 */
  [[nodiscard]] const_reference at(size_type index) const {
    check_index(index);
    return data_[index];
  }

  /** @brief 返回首元素；容器不能为空。 */
  [[nodiscard]] reference front() noexcept { return data_[0]; }
  /** @brief 返回只读首元素；容器不能为空。 */
  [[nodiscard]] const_reference front() const noexcept { return data_[0]; }
  /** @brief 返回末元素；容器不能为空。 */
  [[nodiscard]] reference back() noexcept { return data_[size_ - 1]; }
  /** @brief 返回只读末元素；容器不能为空。 */
  [[nodiscard]] const_reference back() const noexcept { return data_[size_ - 1]; }

  /**
   * @brief 确保容量不小于请求值。
   * @param requested_capacity 最小容量。
   * @throws std::length_error 请求超过 `max_size()` 时抛出。
   */
  void reserve(size_type requested_capacity) {
    if (requested_capacity <= capacity_) {
      return;
    }
    check_capacity(requested_capacity);
    relocate_storage(requested_capacity);
  }

  /**
   * @brief 在尾部原位构造元素。
   * @return 新元素引用；扩容会使原迭代器和引用失效。
   */
  template <typename... Args> reference emplace_back(Args&&... args) {
    if (size_ < capacity_) {
      allocator_type{resource_}.construct(data_ + size_, std::forward<Args>(args)...);
      ++size_;
      return back();
    }

    const size_type new_capacity = recommend_capacity(size_ + 1);
    grow_with_tail(new_capacity, size_ + 1,
                   [&](allocator_type& allocator, pointer target, size_type& constructed) {
                     allocator.construct(target, std::forward<Args>(args)...);
                     ++constructed;
                   });
    return back();
  }

  /** @brief 在尾部复制构造元素。 */
  void push_back(const value_type& value) { (void)emplace_back(value); }

  /** @brief 在尾部移动构造元素。 */
  void push_back(value_type&& value) { (void)emplace_back(std::move(value)); }

  /** @brief 删除末元素；容器不能为空。 */
  void pop_back() noexcept {
    --size_;
    allocator_type{resource_}.destroy(data_ + size_);
  }

  /** @brief 销毁全部元素并保留当前容量。 */
  void clear() noexcept {
    std::destroy_n(data_, size_);
    size_ = 0;
  }

  /**
   * @brief 使用默认构造元素调整大小。
   * @param requested_size 目标元素数量。
   */
  void resize(size_type requested_size) {
    if (requested_size <= size_) {
      destroy_tail(requested_size);
      return;
    }

    if (requested_size > capacity_) {
      grow_with_tail(recommend_capacity(requested_size), requested_size,
                     [](allocator_type& allocator, pointer target, size_type& constructed) {
                       allocator.construct(target);
                       ++constructed;
                     });
      return;
    }

    construct_tail(requested_size,
                   [](allocator_type& allocator, pointer target) { allocator.construct(target); });
  }

  /**
   * @brief 使用指定值复制构造新增元素并调整大小。
   * @param requested_size 目标元素数量。
   * @param value 新增元素的复制来源；允许引用容器现有元素。
   */
  void resize(size_type requested_size, const value_type& value) {
    if (requested_size <= size_) {
      destroy_tail(requested_size);
      return;
    }

    if (requested_size > capacity_) {
      grow_with_tail(recommend_capacity(requested_size), requested_size,
                     [&value](allocator_type& allocator, pointer target, size_type& constructed) {
                       allocator.construct(target, value);
                       ++constructed;
                     });
      return;
    }

    construct_tail(requested_size, [&value](allocator_type& allocator, pointer target) {
      allocator.construct(target, value);
    });
  }

private:
  static constexpr size_type inline_storage_size =
      InlineCapacity == 0 ? size_type{1} : sizeof(value_type) * InlineCapacity;

  [[nodiscard]] static memory::memory_resource*
  resolve_resource(memory::memory_resource* resource) noexcept {
    return resource != nullptr ? resource : memory::get_default_memory_resource();
  }

  [[nodiscard]] pointer inline_data() noexcept {
    return reinterpret_cast<pointer>(inline_storage_);
  }
  [[nodiscard]] const_pointer inline_data() const noexcept {
    return reinterpret_cast<const_pointer>(inline_storage_);
  }

  void check_index(size_type index) const {
    if (index < size_) {
      return;
    }
#if CAORS_HAS_EXCEPTIONS
    throw std::out_of_range{"caors::small_vector 索引越界"};
#else
    std::abort();
#endif
  }

  void check_capacity(size_type requested_capacity) const {
    if (requested_capacity <= max_size()) {
      return;
    }
#if CAORS_HAS_EXCEPTIONS
    throw std::length_error{"caors::small_vector 容量超过上限"};
#else
    std::abort();
#endif
  }

  [[nodiscard]] size_type recommend_capacity(size_type minimum_capacity) const {
    check_capacity(minimum_capacity);
    if (capacity_ == 0) {
      return minimum_capacity;
    }

    const size_type increment = (std::max)(capacity_ / 2, size_type{1});
    const size_type limit = max_size();
    const size_type grown = capacity_ > limit - increment ? limit : capacity_ + increment;
    return (std::max)(grown, minimum_capacity);
  }

  void relocate_storage(size_type new_capacity) {
    allocator_type allocator{resource_};
    pointer new_data = allocator.allocate(new_capacity);
    size_type constructed = 0;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      for (; constructed < size_; ++constructed) {
        allocator.construct(new_data + constructed, std::move_if_noexcept(data_[constructed]));
      }
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      std::destroy_n(new_data, constructed);
      allocator.deallocate(new_data, new_capacity);
      throw;
    }
#endif

    std::destroy_n(data_, size_);
    if (!uses_inline_storage()) {
      allocator.deallocate(data_, capacity_);
    }
    data_ = new_data;
    capacity_ = new_capacity;
  }

  template <typename TailConstructor>
  void grow_with_tail(size_type new_capacity, size_type new_size, TailConstructor&& construct_one) {
    allocator_type allocator{resource_};
    pointer new_data = allocator.allocate(new_capacity);
    size_type tail_constructed = 0;
    size_type moved_constructed = 0;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      for (size_type index = size_; index < new_size; ++index) {
        construct_one(allocator, new_data + index, tail_constructed);
      }
      for (; moved_constructed < size_; ++moved_constructed) {
        allocator.construct(new_data + moved_constructed,
                            std::move_if_noexcept(data_[moved_constructed]));
      }
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      std::destroy_n(new_data, moved_constructed);
      std::destroy_n(new_data + size_, tail_constructed);
      allocator.deallocate(new_data, new_capacity);
      throw;
    }
#endif

    std::destroy_n(data_, size_);
    if (!uses_inline_storage()) {
      allocator.deallocate(data_, capacity_);
    }
    data_ = new_data;
    size_ = new_size;
    capacity_ = new_capacity;
  }

  template <typename Constructor>
  void construct_tail(size_type new_size, Constructor&& construct_one) {
    allocator_type allocator{resource_};
    const size_type original_size = size_;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      while (size_ < new_size) {
        construct_one(allocator, data_ + size_);
        ++size_;
      }
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      std::destroy(data_ + original_size, data_ + size_);
      size_ = original_size;
      throw;
    }
#endif
  }

  void destroy_tail(size_type new_size) noexcept {
    std::destroy(data_ + new_size, data_ + size_);
    size_ = new_size;
  }

  pointer data_ = nullptr;
  size_type size_ = 0;
  size_type capacity_ = InlineCapacity;
  memory::memory_resource* resource_ = nullptr;
  alignas(value_type) std::byte inline_storage_[inline_storage_size]{};
};

} // namespace caors::core

#endif
