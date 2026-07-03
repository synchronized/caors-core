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
    if (data_ == nullptr || storage_ == nullptr || !data_->value_type) {
      return false;
    }

    if (data_->kind != meta::type_kind::raw_pointer && data_->kind != meta::type_kind::wrapper) {
      return false;
    }

    return data_->indirect_value != nullptr || data_->mutable_indirect_value != nullptr ||
           data_->has_indirect_value != nullptr || data_->reset_indirect_value != nullptr ||
           data_->ensure_indirect_value != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }
  [[nodiscard]] type_id value_type() const noexcept {
    return valid() ? data_->value_type : type_id{};
  }

  [[nodiscard]] string_name value_type_name() const noexcept {
    const std::optional<meta::type_view> meta =
        valid() ? caors::meta::find(data_->value_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 判断 wrapper 当前是否持有内部值。 */
  [[nodiscard]] bool has_value() const noexcept {
    if (!valid() || data_->has_indirect_value == nullptr) {
      return false;
    }

    return data_->has_indirect_value(storage_);
  }

  /** @brief 获取内部值引用。 */
  [[nodiscard]] any_ref value_ref() const {
    if (!valid() || data_->indirect_value == nullptr) {
      return {};
    }

    const void* value = nullptr;
    bool value_writable = false;
    if (writable() && data_->mutable_indirect_value != nullptr) {
      value = data_->mutable_indirect_value(const_cast<void*>(storage_));
      value_writable = value != nullptr;
    } else {
      value = data_->indirect_value(storage_);
    }

    if (value == nullptr) {
      return {};
    }

    return any_ref::from_raw(data_->value_type, value, value_writable);
  }

  /** @brief 重置 wrapper 或裸指针为空。 */
  bool reset() const {
    if (!writable() || data_->reset_indirect_value == nullptr) {
      return false;
    }

    return data_->reset_indirect_value(const_cast<void*>(storage_));
  }

  /** @brief 确保 wrapper 持有内部值。裸指针不会自动分配内存。 */
  bool ensure_value() const {
    if (!writable() || data_->ensure_indirect_value == nullptr) {
      return false;
    }

    return data_->ensure_indirect_value(const_cast<void*>(storage_));
  }

private:
  friend class any;
  friend class any_ref;

  indirect_view(const meta::type_semantics_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const meta::type_semantics_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#include "caors/value/view_factory.h"

#endif
