// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_PAIR_VIEW_H_
#define CAORS_VALUE_PAIR_VIEW_H_

#include "caors/value/any_value.h"

namespace caors::value {

/**
 * @brief 二元组运行期视图。
 *
 * `pair_view` 不拥有二元组，只引用 `any` 或 `any_ref` 指向的 pair 对象。first 通常用于 key
 * 语义，second 作为统一 value 语义暴露。
 */
class pair_view final {
public:
  pair_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询 first 类型。 */
  [[nodiscard]] type_id first_type() const noexcept {
    return valid() ? data_->key_type : type_id{};
  }

  /** @brief 查询 first 类型名。 */
  [[nodiscard]] string_name first_type_name() const noexcept {
    const std::optional<meta::type_view> meta =
        valid() ? caors::meta::find(data_->key_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 查询 second 的统一 value 类型。 */
  [[nodiscard]] type_id value_type() const noexcept {
    return valid() ? data_->value_type : type_id{};
  }

  /** @brief 查询 second 的统一 value 类型名。 */
  [[nodiscard]] string_name value_type_name() const noexcept {
    const std::optional<meta::type_view> meta =
        valid() ? caors::meta::find(data_->value_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 查询 second 类型，等价于 `value_type()`。 */
  [[nodiscard]] type_id second_type() const noexcept { return value_type(); }

  /** @brief 查询 second 类型名，等价于 `value_type_name()`。 */
  [[nodiscard]] string_name second_type_name() const noexcept { return value_type_name(); }

  /** @brief 获取 first 引用。 */
  [[nodiscard]] any_ref first_ref() const {
    if (!valid() || data_->first == nullptr) {
      return {};
    }

    const void* value = nullptr;
    bool value_writable = false;
    if (writable() && data_->mutable_first != nullptr) {
      value = data_->mutable_first(const_cast<void*>(storage_));
      value_writable = value != nullptr;
    } else {
      value = data_->first(storage_);
    }

    if (value == nullptr) {
      return {};
    }

    return any_ref::from_raw(data_->key_type, value, value_writable);
  }

  /** @brief 获取 second 引用。 */
  [[nodiscard]] any_ref second_ref() const {
    if (!valid() || data_->second == nullptr) {
      return {};
    }

    const void* value = nullptr;
    bool value_writable = false;
    if (writable() && data_->mutable_second != nullptr) {
      value = data_->mutable_second(const_cast<void*>(storage_));
      value_writable = value != nullptr;
    } else {
      value = data_->second(storage_);
    }

    if (value == nullptr) {
      return {};
    }

    return any_ref::from_raw(data_->value_type, value, value_writable);
  }

  /** @brief 获取 second 引用，等价于 `second_ref()`。 */
  [[nodiscard]] any_ref value_ref() const { return second_ref(); }

  /** @brief 写入 first。 */
  bool set_first(const any& value) const {
    if (!writable() || !value.valid() || value.type() != data_->key_type) {
      return false;
    }

    any_ref target = first_ref();
    return target.valid() && target.try_set(value);
  }

  /** @brief 写入 second。 */
  bool set_second(const any& value) const {
    if (!writable() || !value.valid() || value.type() != data_->value_type) {
      return false;
    }

    any_ref target = second_ref();
    return target.valid() && target.try_set(value);
  }

private:
  friend class any;
  friend class any_ref;

  pair_view(const meta::detail::pair_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const meta::detail::pair_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#endif
