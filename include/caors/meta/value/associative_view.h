// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_ASSOCIATIVE_VIEW_H_
#define CAORS_META_VALUE_ASSOCIATIVE_VIEW_H_

#include "caors/meta/value/fwd.h"

namespace caors::meta {

/**
 * @brief 关联容器运行期视图。
 *
 * `associative_view` 不拥有容器，只引用 `any` 或 `any_ref`
 * 指向的关联容器对象。只读视图允许查询和遍历； 可写视图才允许清空和插入。
 */
class CAORS_API associative_view final {
public:
  class CAORS_API iterator final {
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

    friend CAORS_API bool operator==(const iterator& lhs, const iterator& rhs) noexcept;
    friend CAORS_API bool operator!=(const iterator& lhs, const iterator& rhs) noexcept;

  private:
    friend class associative_view;

    iterator(meta::type type, const meta::associative_ops* ops, void* iterator) noexcept;

    void swap(iterator& other) noexcept;
    void reset() noexcept;

    meta::type type_{};
    const meta::associative_ops* ops_ = nullptr;
    void* iterator_ = nullptr;
  };

  associative_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  [[nodiscard]] meta::type get_key_type() const noexcept;

  /** @brief 查询关联容器 value 类型。 */
  [[nodiscard]] meta::type get_value_type() const noexcept;

  /** @brief 查询关联容器 mapped 类型，等价于 `get_value_type()`。 */
  [[nodiscard]] meta::type get_mapped_type() const noexcept { return get_value_type(); }

  /** @brief 获取关联容器元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const;

  [[nodiscard]] iterator begin() const;
  [[nodiscard]] iterator end() const;

  /** @brief 清空关联容器。 */
  bool clear() const;

  /** @brief 插入一组 key/value。 */
  bool insert_value(const any& key, const any& mapped) const;

private:
  friend class any;
  friend class any_ref;
  friend CAORS_API associative_view make_associative_view(any_ref source) noexcept;
  friend CAORS_API bool operator==(const iterator& lhs, const iterator& rhs) noexcept;
  friend CAORS_API bool operator!=(const iterator& lhs, const iterator& rhs) noexcept;

  associative_view(meta::type type, const void* storage, bool writable) noexcept;

  [[nodiscard]] bool has_iterator_support() const noexcept;
  [[nodiscard]] const meta::associative_ops* data() const noexcept;

  meta::type type_{};
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::meta

#endif
