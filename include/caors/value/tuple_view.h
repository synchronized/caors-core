// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_TUPLE_VIEW_H_
#define CAORS_VALUE_TUPLE_VIEW_H_

#include "caors/value/any_value.h"

namespace caors::value {

/**
 * @brief tuple-like 运行期视图。
 *
 * `tuple_view` 不拥有 tuple，只引用 `any` 或 `any_ref` 指向的 tuple-like
 * 对象。元素类型可以逐个查询， 元素值通过下标暴露为 `any_ref`。
 */
class tuple_view final {
public:
  tuple_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询 tuple 元素数量。 */
  [[nodiscard]] std::size_t size() const noexcept { return valid() ? data_->elements.size() : 0; }

  /** @brief 查询指定元素类型。 */
  [[nodiscard]] std::optional<type_id> element_type(std::size_t index) const noexcept {
    if (!valid() || index >= data_->elements.size()) {
      return std::nullopt;
    }

    return data_->elements[index].value_type;
  }

  /** @brief 查询指定元素类型名。 */
  [[nodiscard]] string_name element_type_name(std::size_t index) const noexcept {
    const std::optional<type_id> id = element_type(index);
    const meta::type_view meta = id.has_value() ? caors::meta::find(*id) : meta::type_view{};
    return meta ? meta.name() : string_name{};
  }

  /** @brief 获取指定元素引用。 */
  [[nodiscard]] any_ref element_ref(std::size_t index) const {
    if (!valid() || index >= data_->elements.size()) {
      return {};
    }

    const meta::detail::tuple_element_data& element_data = data_->elements[index];
    const void* value = nullptr;
    bool value_writable = false;
    if (writable() && element_data.mutable_element != nullptr) {
      value = element_data.mutable_element(const_cast<void*>(storage_));
      value_writable = value != nullptr;
    } else if (element_data.element != nullptr) {
      value = element_data.element(storage_);
    }

    if (value == nullptr) {
      return {};
    }

    return any_ref::from_raw(element_data.value_type, value, value_writable);
  }

  /** @brief 写入指定元素。 */
  bool set_element(std::size_t index, const any& value) const {
    const std::optional<type_id> expected = element_type(index);
    if (!writable() || !expected.has_value() || !value.valid() || value.type() != *expected) {
      return false;
    }

    any_ref target = element_ref(index);
    return target.valid() && target.try_set(value);
  }

private:
  friend class any;
  friend class any_ref;

  tuple_view(const meta::detail::tuple_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const meta::detail::tuple_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#endif
