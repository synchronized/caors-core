// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_ASSOCIATIVE_VIEW_H_
#define CAORS_VALUE_ASSOCIATIVE_VIEW_H_

#include "caors/value/associative_entry_ref.h"

namespace caors::value {

/**
 * @brief 关联容器运行期视图。
 *
 * `associative_view` 不拥有容器，只引用 `any` 或 `any_ref`
 * 指向的关联容器对象。只读视图允许查询和遍历； 可写视图才允许清空和插入。
 */
class associative_view final {
public:
  class iterator final {
  public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = associative_entry_ref;
    using reference = associative_entry_ref;
    using pointer = void;

    iterator() noexcept = default;
    iterator(const iterator& other);
    iterator(iterator&& other) noexcept;
    ~iterator();

    iterator& operator=(const iterator& other);
    iterator& operator=(iterator&& other) noexcept;

    [[nodiscard]] associative_entry_ref operator*() const;

    iterator& operator++() noexcept;
    iterator operator++(int);

    friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept;
    friend bool operator!=(const iterator& lhs, const iterator& rhs) noexcept;

  private:
    friend class associative_view;

    iterator(const associative_view* view, void* iterator) noexcept;

    void swap(iterator& other) noexcept;
    void reset() noexcept;

    const associative_view* view_ = nullptr;
    void* iterator_ = nullptr;
  };

  associative_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  [[nodiscard]] type_id key_type() const noexcept;
  [[nodiscard]] string_name key_type_name() const noexcept;

  /** @brief 查询关联容器 value 类型。 */
  [[nodiscard]] type_id value_type() const noexcept;

  /** @brief 查询关联容器 value 类型名。 */
  [[nodiscard]] string_name value_type_name() const noexcept;

  /** @brief 查询关联容器 mapped 类型，等价于 `value_type()`。 */
  [[nodiscard]] type_id mapped_type() const noexcept { return value_type(); }

  /** @brief 查询关联容器 mapped 类型名，等价于 `value_type_name()`。 */
  [[nodiscard]] string_name mapped_type_name() const noexcept { return value_type_name(); }

  /** @brief 获取关联容器元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const;

  [[nodiscard]] iterator begin() const;
  [[nodiscard]] iterator end() const;

  /** @brief 遍历关联容器 key/value 只读引用。 */
  bool for_each_ref(associative_entry_visitor visitor, void* user_data = nullptr) const;

  /** @brief 清空关联容器。 */
  bool clear() const;

  /** @brief 插入一组 key/value。 */
  bool insert_value(const any& key, const any& mapped) const;

private:
  friend class any;
  friend class any_ref;
  friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept;
  friend bool operator!=(const iterator& lhs, const iterator& rhs) noexcept;

  associative_view(const meta::detail::type_descriptor* descriptor, const void* storage,
                   bool writable) noexcept;

  [[nodiscard]] bool has_iterator_support() const noexcept;
  [[nodiscard]] const meta::detail::associative_data* data() const noexcept;

  const meta::detail::type_descriptor* descriptor_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#endif
