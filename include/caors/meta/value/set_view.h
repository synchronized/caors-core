// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_SET_VIEW_H_
#define CAORS_META_VALUE_SET_VIEW_H_

#include "caors/meta/value/fwd.h"

namespace caors::meta {

/**
 * @brief 集合容器运行期视图。
 *
 * `set_view` 不拥有容器，只引用 `any` 或 `any_ref` 指向的集合容器对象。集合元素按 key
 * 语义暴露为只读引用； 可写视图允许清空和插入元素。
 */
class set_view final {
public:
  class iterator final {
  public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = any_ref;
    using reference = any_ref;
    using pointer = void;

    iterator() noexcept = default;
    iterator(const iterator& other);
    iterator(iterator&& other) noexcept;
    ~iterator();

    iterator& operator=(const iterator& other);
    iterator& operator=(iterator&& other) noexcept;

    [[nodiscard]] any_ref operator*() const;

    iterator& operator++() noexcept;
    iterator operator++(int);

    friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept;
    friend bool operator!=(const iterator& lhs, const iterator& rhs) noexcept;

  private:
    friend class set_view;

    iterator(const set_view* view, void* iterator) noexcept;

    void swap(iterator& other) noexcept;
    void reset() noexcept;

    const set_view* view_ = nullptr;
    void* iterator_ = nullptr;
  };

  set_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询集合元素的统一 value 类型。 */
  [[nodiscard]] meta::type get_value_type() const noexcept;

  /** @brief 查询集合元素类型，等价于 `get_value_type()`。 */
  [[nodiscard]] meta::type get_element_type() const noexcept { return get_value_type(); }

  /** @brief 获取集合元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const;

  [[nodiscard]] iterator begin() const;
  [[nodiscard]] iterator end() const;

  /** @brief 清空集合。 */
  bool clear() const;

  /** @brief 插入一个元素。 */
  bool insert_value(const any& value) const;

private:
  friend class any;
  friend class any_ref;
  friend set_view make_set_view(any_ref source) noexcept;
  friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept;
  friend bool operator!=(const iterator& lhs, const iterator& rhs) noexcept;

  set_view(meta::type type, const void* storage, bool writable) noexcept;

  [[nodiscard]] bool has_iterator_support() const noexcept;
  [[nodiscard]] const meta::set_ops* data() const noexcept;

  meta::type type_{};
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::meta

#endif
