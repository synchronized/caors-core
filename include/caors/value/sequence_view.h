// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_SEQUENCE_VIEW_H_
#define CAORS_VALUE_SEQUENCE_VIEW_H_

#include "caors/value/fwd.h"

namespace caors::value {

/**
 * @brief 顺序容器运行期视图。
 *
 * `sequence_view` 不拥有容器，只引用 `any` 或 `any_ref`
 * 指向的顺序容器对象。只读视图允许查询和访问元素； 可写视图才允许清空、调整大小和追加元素。
 */
class sequence_view final {
public:
  class iterator final {
  public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = any_ref;
    using reference = any_ref;
    using pointer = void;

    iterator() noexcept = default;

    [[nodiscard]] any_ref operator*() const;

    iterator& operator++() noexcept;
    iterator operator++(int) noexcept;

    friend bool operator==(iterator lhs, iterator rhs) noexcept;
    friend bool operator!=(iterator lhs, iterator rhs) noexcept;

  private:
    friend class sequence_view;

    iterator(const sequence_view* view, std::size_t index) noexcept;

    const sequence_view* view_ = nullptr;
    std::size_t index_ = 0;
  };

  sequence_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询顺序容器元素的统一 value 类型。 */
  [[nodiscard]] meta::type get_value_type() const noexcept;

  /** @brief 查询顺序容器元素类型，等价于 `get_value_type()`。 */
  [[nodiscard]] meta::type get_element_type() const noexcept { return get_value_type(); }

  /** @brief 获取顺序容器元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const;

  [[nodiscard]] iterator begin() const;
  [[nodiscard]] iterator end() const;

  /** @brief 获取指定元素引用。 */
  [[nodiscard]] any_ref element_ref(std::size_t index) const;

  /** @brief 清空顺序容器。 */
  bool clear() const;

  /** @brief 调整顺序容器大小。 */
  bool resize(std::size_t size) const;

  /** @brief 使用指定资源调整顺序容器大小。 */
  bool resize(std::size_t size, memory::memory_resource* resource) const;

  /** @brief 追加一个元素。 */
  bool append_value(const any& value) const;

private:
  friend class any;
  friend class any_ref;

  sequence_view(const meta::detail::type_descriptor* descriptor, const void* storage,
                bool writable) noexcept;

  [[nodiscard]] const meta::detail::sequence_data* data() const noexcept;

  const meta::detail::type_descriptor* descriptor_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#endif
