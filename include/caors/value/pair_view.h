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
    const meta::detail::pair_data* info = data();
    return info != nullptr && info->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询 first 类型。 */
  [[nodiscard]] type_id first_type() const noexcept {
    const meta::detail::pair_data* info = data();
    return valid() ? info->key_type : type_id{};
  }

  /** @brief 查询 first 类型名。 */
  [[nodiscard]] string_name first_type_name() const noexcept {
    const meta::type meta = valid() ? caors::meta::find(data()->key_type) : meta::type{};
    return meta ? meta.name() : string_name{};
  }

  /** @brief 查询 second 的统一 value 类型。 */
  [[nodiscard]] type_id value_type() const noexcept {
    const meta::detail::pair_data* info = data();
    return valid() ? info->value_type : type_id{};
  }

  /** @brief 查询 second 的统一 value 类型名。 */
  [[nodiscard]] string_name value_type_name() const noexcept {
    const meta::type meta = valid() ? caors::meta::find(data()->value_type) : meta::type{};
    return meta ? meta.name() : string_name{};
  }

  /** @brief 查询 second 类型，等价于 `value_type()`。 */
  [[nodiscard]] type_id second_type() const noexcept { return value_type(); }

  /** @brief 查询 second 类型名，等价于 `value_type_name()`。 */
  [[nodiscard]] string_name second_type_name() const noexcept { return value_type_name(); }

  /** @brief 获取 first 引用。 */
  [[nodiscard]] any_ref first_ref() const {
    const meta::detail::pair_data* info = data();
    if (!valid() || info->first == nullptr) {
      return {};
    }

    const void* value = nullptr;
    bool value_writable = false;
    if (writable() && info->mutable_first != nullptr) {
      value = info->mutable_first(const_cast<void*>(storage_));
      value_writable = value != nullptr;
    } else {
      value = info->first(storage_);
    }

    if (value == nullptr) {
      return {};
    }

    return any_ref::from_raw(info->key_type, info->key_descriptor, value, value_writable);
  }

  /** @brief 获取 second 引用。 */
  [[nodiscard]] any_ref second_ref() const {
    const meta::detail::pair_data* info = data();
    if (!valid() || info->second == nullptr) {
      return {};
    }

    const void* value = nullptr;
    bool value_writable = false;
    if (writable() && info->mutable_second != nullptr) {
      value = info->mutable_second(const_cast<void*>(storage_));
      value_writable = value != nullptr;
    } else {
      value = info->second(storage_);
    }

    if (value == nullptr) {
      return {};
    }

    return any_ref::from_raw(info->value_type, info->value_descriptor, value, value_writable);
  }

  /** @brief 获取 second 引用，等价于 `second_ref()`。 */
  [[nodiscard]] any_ref value_ref() const { return second_ref(); }

  /** @brief 写入 first。 */
  bool set_first(const any& value) const {
    const meta::detail::pair_data* info = data();
    if (!writable() || !value.valid() || value.type() != info->key_type) {
      return false;
    }

    any_ref target = first_ref();
    return target.valid() && target.try_set(value);
  }

  /** @brief 写入 second。 */
  bool set_second(const any& value) const {
    const meta::detail::pair_data* info = data();
    if (!writable() || !value.valid() || value.type() != info->value_type) {
      return false;
    }

    any_ref target = second_ref();
    return target.valid() && target.try_set(value);
  }

private:
  friend class any;
  friend class any_ref;

  pair_view(const meta::detail::type_descriptor* descriptor, const void* storage,
            bool writable) noexcept
      : descriptor_(descriptor), storage_(storage), writable_(writable) {}

  [[nodiscard]] const meta::detail::pair_data* data() const noexcept {
    return descriptor_ != nullptr ? descriptor_->pair : nullptr;
  }

  const meta::detail::type_descriptor* descriptor_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#endif
