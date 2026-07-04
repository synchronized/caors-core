// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_VARIANT_VIEW_H_
#define CAORS_VALUE_VARIANT_VIEW_H_

#include "caors/value/any_value.h"

namespace caors::value {

/**
 * @brief variant-like 运行期视图。
 *
 * `variant_view` 不拥有 variant，只引用 `any` 或 `any_ref` 指向的 variant-like 对象。视图使用
 * 当前候选下标定位真实值，序列化层会按 `[index, value]` 形式保存。
 */
class variant_view final {
public:
  variant_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询候选类型数量。 */
  [[nodiscard]] std::size_t size() const noexcept {
    return valid() ? data_->alternatives.size() : 0;
  }

  /** @brief 查询当前候选下标。variant 处于 valueless 状态时返回空。 */
  [[nodiscard]] std::optional<std::size_t> index() const noexcept {
    if (!valid() || data_->index == nullptr) {
      return std::nullopt;
    }

    const std::size_t active = data_->index(storage_);
    if (active >= data_->alternatives.size()) {
      return std::nullopt;
    }

    return active;
  }

  /** @brief 查询指定候选类型。 */
  [[nodiscard]] std::optional<type_id> alternative_type(std::size_t alternative) const noexcept {
    if (!valid() || alternative >= data_->alternatives.size()) {
      return std::nullopt;
    }

    return data_->alternatives[alternative].value_type;
  }

  /** @brief 查询指定候选类型名。 */
  [[nodiscard]] string_name alternative_type_name(std::size_t alternative) const noexcept {
    const std::optional<type_id> id = alternative_type(alternative);
    const meta::type meta = id.has_value() ? caors::meta::find(*id) : meta::type{};
    return meta ? meta.name() : string_name{};
  }

  /** @brief 查询当前值类型。 */
  [[nodiscard]] type_id value_type() const noexcept {
    const std::optional<std::size_t> active = index();
    if (!active.has_value()) {
      return {};
    }

    return data_->alternatives[*active].value_type;
  }

  /** @brief 查询当前值类型名。 */
  [[nodiscard]] string_name value_type_name() const noexcept {
    const meta::type meta = caors::meta::find(value_type());
    return meta ? meta.name() : string_name{};
  }

  /** @brief 获取当前候选值引用。 */
  [[nodiscard]] any_ref value_ref() const {
    const std::optional<std::size_t> active = index();
    if (!active.has_value()) {
      return {};
    }

    const meta::detail::variant_alternative_data& alternative_data = data_->alternatives[*active];
    const void* value = nullptr;
    bool value_writable = false;
    if (writable() && alternative_data.mutable_alternative != nullptr) {
      value = alternative_data.mutable_alternative(const_cast<void*>(storage_));
      value_writable = value != nullptr;
    } else if (alternative_data.alternative != nullptr) {
      value = alternative_data.alternative(storage_);
    }

    if (value == nullptr) {
      return {};
    }

    return any_ref::from_raw(alternative_data.value_type, alternative_data.value_descriptor, value,
                             value_writable);
  }

  /** @brief 切换当前候选。已处于目标候选时直接成功。 */
  bool emplace(std::size_t alternative) const {
    if (!writable() || alternative >= data_->alternatives.size()) {
      return false;
    }

    const std::optional<std::size_t> active = index();
    if (active.has_value() && *active == alternative) {
      return true;
    }

    const meta::detail::variant_alternative_data& alternative_data =
        data_->alternatives[alternative];
    if (alternative_data.emplace == nullptr) {
      return false;
    }

    return alternative_data.emplace(const_cast<void*>(storage_));
  }

  /** @brief 写入当前候选值。 */
  bool set_value(const any& value) const {
    const type_id expected = value_type();
    if (!writable() || !expected || !value.valid() || value.type() != expected) {
      return false;
    }

    any_ref target = value_ref();
    return target.valid() && target.try_set(value);
  }

private:
  friend class any;
  friend class any_ref;

  variant_view(const meta::detail::variant_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const meta::detail::variant_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#endif
