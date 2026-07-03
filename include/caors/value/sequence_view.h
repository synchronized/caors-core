// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_SEQUENCE_VIEW_H_
#define CAORS_VALUE_SEQUENCE_VIEW_H_

#include "caors/value/any_value.h"

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

    [[nodiscard]] any_ref operator*() const {
      return view_ != nullptr ? view_->element_ref(index_) : any_ref{};
    }

    iterator& operator++() noexcept {
      ++index_;
      return *this;
    }

    iterator operator++(int) noexcept {
      iterator previous = *this;
      ++(*this);
      return previous;
    }

    friend bool operator==(iterator lhs, iterator rhs) noexcept {
      return lhs.view_ == rhs.view_ && lhs.index_ == rhs.index_;
    }

    friend bool operator!=(iterator lhs, iterator rhs) noexcept { return !(lhs == rhs); }

  private:
    friend class sequence_view;

    iterator(const sequence_view* view, std::size_t index) noexcept : view_(view), index_(index) {}

    const sequence_view* view_ = nullptr;
    std::size_t index_ = 0;
  };

  sequence_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询顺序容器元素的统一 value 类型。 */
  [[nodiscard]] type_id value_type() const noexcept {
    return valid() ? data_->value_type : type_id{};
  }

  /** @brief 查询顺序容器元素的统一 value 类型名。 */
  [[nodiscard]] string_name value_type_name() const noexcept {
    const std::optional<meta::type_view> meta =
        valid() ? caors::meta::find(data_->value_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 查询顺序容器元素类型，等价于 `value_type()`。 */
  [[nodiscard]] type_id element_type() const noexcept { return value_type(); }

  /** @brief 查询顺序容器元素类型名，等价于 `value_type_name()`。 */
  [[nodiscard]] string_name element_type_name() const noexcept { return value_type_name(); }

  /** @brief 获取顺序容器元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const {
    if (!valid() || data_->size == nullptr) {
      return std::nullopt;
    }

    return data_->size(storage_);
  }

  [[nodiscard]] iterator begin() const { return valid() ? iterator{this, 0} : iterator{}; }

  [[nodiscard]] iterator end() const {
    const std::optional<std::size_t> count = size();
    return count.has_value() ? iterator{this, *count} : iterator{};
  }

  /** @brief 获取指定元素引用。 */
  [[nodiscard]] any_ref element_ref(std::size_t index) const {
    if (!valid() || data_->element == nullptr) {
      return {};
    }

    const void* element = nullptr;
    bool element_writable = false;
    if (writable() && data_->mutable_element != nullptr) {
      element = data_->mutable_element(const_cast<void*>(storage_), index);
      element_writable = element != nullptr;
    } else {
      element = data_->element(storage_, index);
    }

    if (element == nullptr) {
      return {};
    }

    return any_ref::from_raw(data_->value_type, element, element_writable);
  }

  /** @brief 清空顺序容器。 */
  bool clear() const {
    if (!writable() || data_->clear == nullptr) {
      return false;
    }

    data_->clear(const_cast<void*>(storage_));
    return true;
  }

  /** @brief 调整顺序容器大小。 */
  bool resize(std::size_t size) const {
    if (!writable() || data_->resize == nullptr) {
      return false;
    }

    return data_->resize(const_cast<void*>(storage_), size);
  }

  /** @brief 追加一个元素。 */
  bool append_value(const any& value) const {
    if (!writable() || data_->append == nullptr || !value.valid() ||
        value.type() != data_->value_type) {
      return false;
    }

    return data_->append(const_cast<void*>(storage_), value.data());
  }

private:
  friend class any;
  friend class any_ref;

  sequence_view(const meta::detail::sequence_data* data, const void* storage,
                bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const meta::detail::sequence_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#endif
