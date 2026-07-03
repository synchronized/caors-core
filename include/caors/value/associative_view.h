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

    iterator(const iterator& other) : view_(other.view_) {
      if (other.iterator_ != nullptr && view_ != nullptr && view_->data_ != nullptr &&
          view_->data_->iterator_clone != nullptr) {
        iterator_ = view_->data_->iterator_clone(other.iterator_);
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

    [[nodiscard]] associative_entry_ref operator*() const {
      if (view_ == nullptr || view_->data_ == nullptr || iterator_ == nullptr ||
          view_->data_->iterator_key == nullptr || view_->data_->iterator_value == nullptr) {
        return {};
      }

      const void* key = view_->data_->iterator_key(iterator_);
      const void* value = view_->data_->iterator_value(iterator_);
      return associative_entry_ref{
          any_ref::from_raw(view_->data_->key_type, key, false),
          any_ref::from_raw(view_->data_->value_type, value, false),
      };
    }

    iterator& operator++() noexcept {
      if (view_ != nullptr && view_->data_ != nullptr && iterator_ != nullptr &&
          view_->data_->iterator_advance != nullptr) {
        view_->data_->iterator_advance(iterator_);
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

      if (lhs.view_ == nullptr || lhs.view_->data_ == nullptr ||
          lhs.view_->data_->iterator_equal == nullptr) {
        return false;
      }

      return lhs.view_->data_->iterator_equal(lhs.iterator_, rhs.iterator_);
    }

    friend bool operator!=(const iterator& lhs, const iterator& rhs) noexcept {
      return !(lhs == rhs);
    }

  private:
    friend class associative_view;

    iterator(const associative_view* view, void* iterator) noexcept
        : view_(view), iterator_(iterator) {}

    void swap(iterator& other) noexcept {
      using std::swap;
      swap(view_, other.view_);
      swap(iterator_, other.iterator_);
    }

    void reset() noexcept {
      if (view_ != nullptr && view_->data_ != nullptr && iterator_ != nullptr &&
          view_->data_->iterator_destroy != nullptr) {
        view_->data_->iterator_destroy(iterator_);
      }

      view_ = nullptr;
      iterator_ = nullptr;
    }

    const associative_view* view_ = nullptr;
    void* iterator_ = nullptr;
  };

  associative_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  [[nodiscard]] type_id key_type() const noexcept { return valid() ? data_->key_type : type_id{}; }

  [[nodiscard]] string_name key_type_name() const noexcept {
    const std::optional<meta::type_view> meta =
        valid() ? caors::meta::find(data_->key_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 查询关联容器 value 类型。 */
  [[nodiscard]] type_id value_type() const noexcept {
    return valid() ? data_->value_type : type_id{};
  }

  /** @brief 查询关联容器 value 类型名。 */
  [[nodiscard]] string_name value_type_name() const noexcept {
    const std::optional<meta::type_view> meta =
        valid() ? caors::meta::find(data_->value_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 查询关联容器 mapped 类型，等价于 `value_type()`。 */
  [[nodiscard]] type_id mapped_type() const noexcept { return value_type(); }

  /** @brief 查询关联容器 mapped 类型名，等价于 `value_type_name()`。 */
  [[nodiscard]] string_name mapped_type_name() const noexcept { return value_type_name(); }

  /** @brief 获取关联容器元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const {
    if (!valid() || data_->size == nullptr) {
      return std::nullopt;
    }

    return data_->size(storage_);
  }

  [[nodiscard]] iterator begin() const {
    if (!has_iterator_support()) {
      return {};
    }

    return iterator{this, data_->begin(storage_)};
  }

  [[nodiscard]] iterator end() const {
    if (!has_iterator_support()) {
      return {};
    }

    return iterator{this, data_->end(storage_)};
  }

  /** @brief 遍历关联容器 key/value 只读引用。 */
  bool for_each_ref(associative_entry_visitor visitor, void* user_data = nullptr) const {
    if (!valid() || data_->visit == nullptr || visitor == nullptr) {
      return false;
    }

    struct visit_context {
      const meta::detail::associative_data* data = nullptr;
      associative_entry_visitor visitor = nullptr;
      void* user_data = nullptr;
    };

    visit_context context{data_, visitor, user_data};
    return data_->visit(
        storage_,
        [](const void* key, const void* mapped, void* raw_context) -> bool {
          auto* context = static_cast<visit_context*>(raw_context);
          const meta::detail::associative_data* data = context->data;
          any_ref key_ref = any_ref::from_raw(data->key_type, key, false);
          any_ref mapped_ref = any_ref::from_raw(data->value_type, mapped, false);

          return context->visitor(key_ref, mapped_ref, context->user_data);
        },
        &context);
  }

  /** @brief 清空关联容器。 */
  bool clear() const {
    if (!writable() || data_->clear == nullptr) {
      return false;
    }

    data_->clear(const_cast<void*>(storage_));
    return true;
  }

  /** @brief 插入一组 key/value。 */
  bool insert_value(const any& key, const any& mapped) const {
    if (!writable() || data_->insert == nullptr || !key.valid() || !mapped.valid() ||
        key.type() != data_->key_type || mapped.type() != data_->value_type) {
      return false;
    }

    return data_->insert(const_cast<void*>(storage_), key.data(), mapped.data());
  }

private:
  friend class any;
  friend class any_ref;

  associative_view(const meta::detail::associative_data* data, const void* storage,
                   bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  [[nodiscard]] bool has_iterator_support() const noexcept {
    return valid() && data_->begin != nullptr && data_->end != nullptr &&
           data_->iterator_clone != nullptr && data_->iterator_destroy != nullptr &&
           data_->iterator_advance != nullptr && data_->iterator_equal != nullptr &&
           data_->iterator_key != nullptr && data_->iterator_value != nullptr;
  }

  const meta::detail::associative_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#endif
