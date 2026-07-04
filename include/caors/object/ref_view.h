// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_REF_VIEW_H_
#define CAORS_OBJECT_REF_VIEW_H_

#include "caors/object/ref.h"
#include "caors/value/any_ref.h"

namespace caors::objects {

/**
 * @brief 对象引用运行期视图。
 *
 * `object_ref_view` 不拥有对象，只引用 `any_ref` 指向的对象引用字段。它用于把对象引用语义从通用
 * wrapper 语义中拆出来，调用方可通过该视图按对象身份读写引用。
 */
class object_ref_view final {
public:
  object_ref_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 获取当前引用的对象指针；空引用返回 `nullptr`。 */
  [[nodiscard]] object* get() const noexcept {
    if (!valid() || data_->get == nullptr) {
      return nullptr;
    }

    return data_->get(storage_);
  }

  /** @brief 写入对象指针；类型不匹配、只读或空视图时返回 `false`。 */
  bool set(object* instance) const noexcept {
    if (!writable() || data_->set == nullptr) {
      return false;
    }

    return data_->set(const_cast<void*>(storage_), instance);
  }

private:
  friend object_ref_view object_ref(value::any_ref source) noexcept;

  object_ref_view(const detail::object_ref_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const detail::object_ref_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

/** @brief 如果运行期引用具备对象引用语义，则创建对象引用视图。 */
[[nodiscard]] inline object_ref_view object_ref(value::any_ref source) noexcept {
  if (!source.valid()) {
    return {};
  }

  const meta::detail::type_descriptor* descriptor = value::detail::descriptor_of(source);
  const detail::object_ref_data* data = descriptor != nullptr ? descriptor->object_ref : nullptr;
  if (data == nullptr) {
    return {};
  }

  return object_ref_view{data, source.data(), source.writable()};
}

/** @brief 判断运行期引用是否具备对象引用语义。 */
[[nodiscard]] inline bool is_object_ref(value::any_ref source) noexcept {
  return static_cast<bool>(object_ref(source));
}

} // namespace caors::objects

#endif
