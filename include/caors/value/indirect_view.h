// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_INDIRECT_VIEW_H_
#define CAORS_VALUE_INDIRECT_VIEW_H_

#include "caors/value/any_value.h"

namespace caors::value {

/**
 * @brief 指针和 wrapper 的内部值视图。
 *
 * `indirect_view` 不拥有 wrapper 或内部值，只引用 `any` / `any_ref` 指向的 wrapper 对象。
 * 可写视图才允许重置 wrapper 或返回可写内部值引用。
 */
class indirect_view final {
public:
  indirect_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    const meta::detail::indirect_data* info = data();
    if (descriptor_ == nullptr || info == nullptr || storage_ == nullptr ||
        !meta::detail::descriptor_value_type(*descriptor_)) {
      return false;
    }

    if (!descriptor_->indirect) {
      return false;
    }

    if (descriptor_->kind == meta::type_kind::wrapper &&
        descriptor_->wrapper_semantics == meta::wrapper_semantics::unsupported) {
      return false;
    }

    return info->valid();
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }
  [[nodiscard]] type_id value_type() const noexcept {
    return valid() ? meta::detail::descriptor_value_type(*descriptor_) : type_id{};
  }

  [[nodiscard]] string_name value_type_name() const noexcept {
    const meta::type meta = valid() ? caors::meta::find(value_type()) : meta::type{};
    return meta ? meta.name() : string_name{};
  }

  /** @brief 判断 wrapper 当前是否持有内部值。 */
  [[nodiscard]] bool has_value() const noexcept {
    const meta::detail::indirect_data* info = data();
    if (!valid() || info->has_value == nullptr) {
      return false;
    }

    return info->has_value(storage_);
  }

  /** @brief 获取内部值引用。 */
  [[nodiscard]] any_ref value_ref() const {
    const meta::detail::indirect_data* info = data();
    if (!valid() || info->value == nullptr) {
      return {};
    }

    const void* value = nullptr;
    bool value_writable = false;
    if (writable() && info->mutable_value != nullptr) {
      value = info->mutable_value(const_cast<void*>(storage_));
      value_writable = value != nullptr;
    } else {
      value = info->value(storage_);
    }

    if (value == nullptr) {
      return {};
    }

    return any_ref::from_raw(value_type(), info->value_descriptor, value, value_writable);
  }

  /** @brief 重置 wrapper 或裸指针为空。 */
  bool reset() const {
    const meta::detail::indirect_data* info = data();
    if (!writable() || info->reset == nullptr) {
      return false;
    }

    return info->reset(const_cast<void*>(storage_));
  }

  /** @brief 确保 wrapper 持有内部值。裸指针不会自动分配内存。 */
  bool ensure_value() const {
    const meta::detail::indirect_data* info = data();
    if (!writable() || info->ensure_value == nullptr) {
      return false;
    }

    return info->ensure_value(const_cast<void*>(storage_));
  }

private:
  friend class any;
  friend class any_ref;

  indirect_view(const meta::detail::type_descriptor* descriptor, const void* storage,
                bool writable) noexcept
      : descriptor_(descriptor), storage_(storage), writable_(writable) {}

  [[nodiscard]] const meta::detail::indirect_data* data() const noexcept {
    return descriptor_ != nullptr ? descriptor_->indirect_access : nullptr;
  }

  const meta::detail::type_descriptor* descriptor_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#endif
