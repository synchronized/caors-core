// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_SET_VIEW_H_
#define CAORS_VALUE_SET_VIEW_H_

#include "caors/value/any_value.h"

namespace caors::value {

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

    iterator(const iterator& other) : view_(other.view_) {
      const meta::detail::set_data* info = view_ != nullptr ? view_->data() : nullptr;
      if (other.iterator_ != nullptr && info != nullptr && info->iterator_clone != nullptr) {
        iterator_ = info->iterator_clone(other.iterator_);
      }
    }

    iterator(iterator&& other) noexcept : view_(other.view_), iterator_(other.iterator_) {
      other.view_ = nullptr;
      other.iterator_ = nullptr;
    }

    ~iterator() { reset(); }

    iterator& operator=(const iterator& other) {
      if (this == &other) {
        return *this;
      }

      iterator copy{other};
      swap(copy);
      return *this;
    }

    iterator& operator=(iterator&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      reset();
      view_ = other.view_;
      iterator_ = other.iterator_;
      other.view_ = nullptr;
      other.iterator_ = nullptr;
      return *this;
    }

    [[nodiscard]] any_ref operator*() const {
      const meta::detail::set_data* info = view_ != nullptr ? view_->data() : nullptr;
      if (info == nullptr || iterator_ == nullptr || info->iterator_value == nullptr) {
        return {};
      }

      const void* value = info->iterator_value(iterator_);
      return any_ref::from_raw(info->value_type, info->value_descriptor, value, false);
    }

    iterator& operator++() noexcept {
      const meta::detail::set_data* info = view_ != nullptr ? view_->data() : nullptr;
      if (info != nullptr && iterator_ != nullptr && info->iterator_advance != nullptr) {
        info->iterator_advance(iterator_);
      }

      return *this;
    }

    iterator operator++(int) {
      iterator previous = *this;
      ++(*this);
      return previous;
    }

    friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept {
      if (lhs.view_ != rhs.view_) {
        return lhs.iterator_ == nullptr && rhs.iterator_ == nullptr;
      }

      if (lhs.iterator_ == nullptr || rhs.iterator_ == nullptr) {
        return true;
      }

      const meta::detail::set_data* info = lhs.view_ != nullptr ? lhs.view_->data() : nullptr;
      if (info == nullptr || info->iterator_equal == nullptr) {
        return false;
      }

      return info->iterator_equal(lhs.iterator_, rhs.iterator_);
    }

    friend bool operator!=(const iterator& lhs, const iterator& rhs) noexcept {
      return !(lhs == rhs);
    }

  private:
    friend class set_view;

    iterator(const set_view* view, void* iterator) noexcept : view_(view), iterator_(iterator) {}

    void swap(iterator& other) noexcept {
      using std::swap;
      swap(view_, other.view_);
      swap(iterator_, other.iterator_);
    }

    void reset() noexcept {
      const meta::detail::set_data* info = view_ != nullptr ? view_->data() : nullptr;
      if (info != nullptr && iterator_ != nullptr && info->iterator_destroy != nullptr) {
        info->iterator_destroy(iterator_);
      }

      view_ = nullptr;
      iterator_ = nullptr;
    }

    const set_view* view_ = nullptr;
    void* iterator_ = nullptr;
  };

  set_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    const meta::detail::set_data* info = data();
    return info != nullptr && info->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询集合元素的统一 value 类型。 */
  [[nodiscard]] type_id value_type() const noexcept {
    const meta::detail::set_data* info = data();
    return valid() ? info->value_type : type_id{};
  }

  /** @brief 查询集合元素的统一 value 类型名。 */
  [[nodiscard]] string_name value_type_name() const noexcept {
    const meta::type meta = valid() ? caors::meta::find(data()->value_type) : meta::type{};
    return meta ? meta.name() : string_name{};
  }

  /** @brief 查询集合元素类型，等价于 `value_type()`。 */
  [[nodiscard]] type_id element_type() const noexcept { return value_type(); }

  /** @brief 查询集合元素类型名，等价于 `value_type_name()`。 */
  [[nodiscard]] string_name element_type_name() const noexcept { return value_type_name(); }

  /** @brief 获取集合元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const {
    const meta::detail::set_data* info = data();
    if (!valid() || info->size == nullptr) {
      return std::nullopt;
    }

    return info->size(storage_);
  }

  [[nodiscard]] iterator begin() const {
    if (!has_iterator_support()) {
      return {};
    }

    return iterator{this, data()->begin(storage_)};
  }

  [[nodiscard]] iterator end() const {
    if (!has_iterator_support()) {
      return {};
    }

    return iterator{this, data()->end(storage_)};
  }

  /** @brief 遍历集合元素只读引用。 */
  bool for_each_ref(set_element_visitor visitor, void* user_data = nullptr) const {
    const meta::detail::set_data* info = data();
    if (!valid() || info->visit == nullptr || visitor == nullptr) {
      return false;
    }

    struct visit_context {
      const meta::detail::set_data* data = nullptr;
      set_element_visitor visitor = nullptr;
      void* user_data = nullptr;
    };

    visit_context context{info, visitor, user_data};
    return info->visit(
        storage_,
        [](const void* element, void* raw_context) -> bool {
          auto* context = static_cast<visit_context*>(raw_context);
          const meta::detail::set_data* data = context->data;
          any_ref element_ref =
              any_ref::from_raw(data->value_type, data->value_descriptor, element, false);

          return context->visitor(element_ref, context->user_data);
        },
        &context);
  }

  /** @brief 清空集合。 */
  bool clear() const {
    const meta::detail::set_data* info = data();
    if (!writable() || info->clear == nullptr) {
      return false;
    }

    info->clear(const_cast<void*>(storage_));
    return true;
  }

  /** @brief 插入一个元素。 */
  bool insert_value(const any& value) const {
    const meta::detail::set_data* info = data();
    if (!writable() || info->insert == nullptr || !value.valid() ||
        value.type() != info->value_type) {
      return false;
    }

    return info->insert(const_cast<void*>(storage_), value.data());
  }

private:
  friend class any;
  friend class any_ref;

  set_view(const meta::detail::type_descriptor* descriptor, const void* storage,
           bool writable) noexcept
      : descriptor_(descriptor), storage_(storage), writable_(writable) {}

  [[nodiscard]] bool has_iterator_support() const noexcept {
    const meta::detail::set_data* info = data();
    return valid() && info->begin != nullptr && info->end != nullptr &&
           info->iterator_clone != nullptr && info->iterator_destroy != nullptr &&
           info->iterator_advance != nullptr && info->iterator_equal != nullptr &&
           info->iterator_value != nullptr;
  }

  [[nodiscard]] const meta::detail::set_data* data() const noexcept {
    return descriptor_ != nullptr ? descriptor_->set : nullptr;
  }

  const meta::detail::type_descriptor* descriptor_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#endif
