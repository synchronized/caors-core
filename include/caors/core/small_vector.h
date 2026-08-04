// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_CORE_SMALL_VECTOR_H_
#define CAORS_CORE_SMALL_VECTOR_H_

#include "caors/memory/resource.h"

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
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

  /** @brief 使用 allocator 所绑定的资源构造空容器。 */
  explicit small_vector(const allocator_type& allocator) noexcept
      : small_vector(allocator.resource()) {}

  /** @brief 从初始化列表复制构造。 */
  small_vector(std::initializer_list<value_type> values,
               memory::memory_resource* resource = nullptr)
      : small_vector(values.begin(), values.end(), resource) {}

  /** @brief 从输入区间构造。 */
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
  small_vector(Iterator first, Sentinel last, memory::memory_resource* resource = nullptr)
      : small_vector(resource) {
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      for (; first != last; ++first) {
        emplace_back(*first);
      }
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      release_storage();
      throw;
    }
#endif
  }

  /** @brief 使用源容器资源复制构造。 */
  small_vector(const small_vector& other) : small_vector(other, other.resource_) {}

  /**
   * @brief 使用指定资源复制构造。
   * @param other 复制来源。
   * @param resource 目标动态存储使用的非拥有资源。
   */
  small_vector(const small_vector& other, memory::memory_resource* resource)
      : small_vector(resource) {
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      copy_elements_from(other);
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      release_storage();
      throw;
    }
#endif
  }

  /** @brief 使用 allocator 所绑定的资源复制构造。 */
  small_vector(const small_vector& other, const allocator_type& allocator)
      : small_vector(other, allocator.resource()) {}

  /** @brief 使用源容器资源移动构造；动态缓冲区可被直接接管。 */
  small_vector(small_vector&& other) : small_vector(other.resource_) {
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      move_elements_from(other);
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      release_storage();
      throw;
    }
#endif
  }

  /**
   * @brief 使用指定资源移动构造。
   * @param other 移动来源。
   * @param resource 目标动态存储使用的非拥有资源；资源等价时允许接管缓冲区。
   */
  small_vector(small_vector&& other, memory::memory_resource* resource) : small_vector(resource) {
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      move_elements_from(other);
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      release_storage();
      throw;
    }
#endif
  }

  /** @brief 使用 allocator 所绑定的资源移动构造。 */
  small_vector(small_vector&& other, const allocator_type& allocator)
      : small_vector(std::move(other), allocator.resource()) {}

  /** @brief 复制赋值并保留目标容器资源。 */
  small_vector& operator=(const small_vector& other) {
    if (this == &other) {
      return *this;
    }

    small_vector temporary{resource_};
    if (!other.empty() && InlineCapacity < max_size()) {
      temporary.reserve((std::max)(other.size_, InlineCapacity + 1));
    }
    temporary.copy_elements_from(other);
    release_storage();
    move_elements_from(temporary);
    return *this;
  }

  /** @brief 移动赋值并传播源容器资源。 */
  small_vector& operator=(small_vector&& other) {
    if (this == &other) {
      return *this;
    }

    release_storage();
    resource_ = other.resource_;
    move_elements_from(other);
    return *this;
  }

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
   * @brief 请求收缩动态容量，并在元素可放入内联区时迁回对象内部。
   *
   * 发生迁移时，全部元素引用和迭代器失效。
   */
  void shrink_to_fit() {
    if (uses_inline_storage()) {
      return;
    }
    if (size_ <= InlineCapacity) {
      relocate_to_inline();
      return;
    }
    if (size_ < capacity_) {
      relocate_storage(size_);
    }
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

  /** @brief 用指定数量的同值元素替换当前内容。 */
  void assign(size_type count, const value_type& value) {
    small_vector replacement{resource_};
    replacement.resize(count, value);
    *this = std::move(replacement);
  }

  /** @brief 用输入区间替换当前内容；允许区间来自当前容器。 */
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
  void assign(Iterator first, Sentinel last) {
    small_vector replacement{first, last, resource_};
    *this = std::move(replacement);
  }

  /** @brief 用初始化列表替换当前内容。 */
  void assign(std::initializer_list<value_type> values) { assign(values.begin(), values.end()); }

  /** @brief 在指定位置原位构造单个元素。 */
  template <typename... Args> iterator emplace(const_iterator position, Args&&... args) {
    const size_type index = position_index(position);
    small_vector<value_type, 1> staged{resource_};
    staged.emplace_back(std::forward<Args>(args)...);
    return insert_staged(index, std::move(staged.front()));
  }

  /** @brief 在指定位置复制插入单个元素。 */
  iterator insert(const_iterator position, const value_type& value) {
    return emplace(position, value);
  }

  /** @brief 在指定位置移动插入单个元素。 */
  iterator insert(const_iterator position, value_type&& value) {
    return emplace(position, std::move(value));
  }

  /** @brief 在指定位置插入指定数量的同值元素。 */
  iterator insert(const_iterator position, size_type count, const value_type& value) {
    const size_type index = position_index(position);
    small_vector staged{resource_};
    staged.resize(count, value);
    return insert_staged_range(index, staged);
  }

  /** @brief 在指定位置插入输入区间；允许区间来自当前容器。 */
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
  iterator insert(const_iterator position, Iterator first, Sentinel last) {
    const size_type index = position_index(position);
    small_vector staged{first, last, resource_};
    return insert_staged_range(index, staged);
  }

  /** @brief 在指定位置插入初始化列表。 */
  iterator insert(const_iterator position, std::initializer_list<value_type> values) {
    return insert(position, values.begin(), values.end());
  }

  /** @brief 擦除指定位置的元素。 */
  iterator erase(const_iterator position) { return erase(position, position + 1); }

  /** @brief 擦除半开区间 `[first, last)`。 */
  iterator erase(const_iterator first, const_iterator last) {
    const size_type first_index = position_index(first);
    const size_type last_index = position_index(last);
    if (last_index < first_index) {
#if CAORS_HAS_EXCEPTIONS
      throw std::out_of_range{"caors::small_vector 擦除区间无效"};
#else
      std::abort();
#endif
    }
    const size_type erased_count = last_index - first_index;
    if (erased_count == 0) {
      return data_ + first_index;
    }
    std::move(data_ + last_index, data_ + size_, data_ + first_index);
    destroy_tail(size_ - erased_count);
    return data_ + first_index;
  }

  /** @brief 交换内容、资源和存储所有权。 */
  void swap(small_vector& other) {
    if (this == &other) {
      return;
    }
    small_vector temporary{std::move(*this)};
    *this = std::move(other);
    other = std::move(temporary);
  }

  friend void swap(small_vector& lhs, small_vector& rhs) { lhs.swap(rhs); }

  /** @brief 按元素比较内容是否相等；资源和容量不参与比较。 */
  friend bool operator==(const small_vector& lhs, const small_vector& rhs)
    requires std::equality_comparable<value_type>
  {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
  }

  /** @brief 按字典序比较元素；资源和容量不参与比较。 */
  friend auto operator<=>(const small_vector& lhs, const small_vector& rhs)
    requires std::three_way_comparable<value_type>
  {
    return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
                                                  std::compare_three_way{});
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

  [[nodiscard]] size_type position_index(const_iterator position) const {
    return static_cast<size_type>(position - cbegin());
  }

  iterator insert_staged(size_type index, value_type&& value) {
    if (index == size_) {
      emplace_back(std::move(value));
      return data_ + index;
    }
    if (size_ == capacity_) {
      reserve(recommend_capacity(size_ + 1));
    }

    allocator_type allocator{resource_};
    allocator.construct(data_ + size_, std::move_if_noexcept(data_[size_ - 1]));
    ++size_;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      std::move_backward(data_ + index, data_ + size_ - 2, data_ + size_ - 1);
      data_[index] = std::move(value);
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      allocator.destroy(data_ + size_ - 1);
      --size_;
      throw;
    }
#endif
    return data_ + index;
  }

  iterator insert_staged_range(size_type index, small_vector& staged) {
    if (staged.empty()) {
      return data_ + index;
    }
    if (staged.size_ > max_size() - size_) {
      check_capacity(max_size() + 1);
    }
    reserve(size_ + staged.size_);
    for (auto& value : staged) {
      insert_staged(index++, std::move(value));
    }
    return data_ + index - staged.size_;
  }

  [[nodiscard]] bool can_adopt_storage_from(const small_vector& other) const noexcept {
    return !other.uses_inline_storage() && *resource_ == *other.resource_;
  }

  void copy_elements_from(const small_vector& other) {
    if (other.size_ > capacity_) {
      reserve(other.size_);
    }
    allocator_type allocator{resource_};
    const size_type original_size = size_;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      while (size_ < other.size_) {
        allocator.construct(data_ + size_, other.data_[size_]);
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

  void move_elements_from(small_vector& other) {
    if (can_adopt_storage_from(other)) {
      data_ = other.data_;
      size_ = other.size_;
      capacity_ = other.capacity_;
      other.reset_to_inline();
      return;
    }

    if (other.size_ > capacity_) {
      reserve(other.size_);
    }
    allocator_type allocator{resource_};
    const size_type original_size = size_;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      while (size_ < other.size_) {
        allocator.construct(data_ + size_, std::move(other.data_[size_]));
        ++size_;
      }
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      std::destroy(data_ + original_size, data_ + size_);
      size_ = original_size;
      throw;
    }
#endif
    other.clear();
  }

  void relocate_to_inline() {
    pointer old_data = data_;
    const size_type old_capacity = capacity_;
    pointer new_data = inline_data();
    allocator_type allocator{resource_};
    size_type constructed = 0;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      for (; constructed < size_; ++constructed) {
        allocator.construct(new_data + constructed, std::move_if_noexcept(old_data[constructed]));
      }
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      std::destroy_n(new_data, constructed);
      throw;
    }
#endif

    std::destroy_n(old_data, size_);
    allocator.deallocate(old_data, old_capacity);
    data_ = new_data;
    capacity_ = InlineCapacity;
  }

  void release_storage() noexcept {
    std::destroy_n(data_, size_);
    if (!uses_inline_storage()) {
      allocator_type{resource_}.deallocate(data_, capacity_);
    }
    reset_to_inline();
  }

  void reset_to_inline() noexcept {
    data_ = inline_data();
    size_ = 0;
    capacity_ = InlineCapacity;
  }

  pointer data_ = nullptr;
  size_type size_ = 0;
  size_type capacity_ = InlineCapacity;
  memory::memory_resource* resource_ = nullptr;
  alignas(value_type) std::byte inline_storage_[inline_storage_size]{};
};

} // namespace caors::core

#endif
