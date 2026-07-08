// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_PTR_VIEW_H_
#define CAORS_OBJECT_PTR_VIEW_H_

#include "caors/meta/capability.h"
#include "caors/meta/detail/type_descriptor.h"
#include "caors/object/ptr.h"
#include "caors/value/any_ref.h"

namespace caors::objects {

/**
 * @brief 对象指针运行期视图。
 *
 * `ptr_view` 不拥有对象，只引用 `any_ref` 指向的对象指针字段。它用于把对象指针语义从通用
 * wrapper 语义中拆出来，调用方可通过该视图按对象身份读写指针。
 */
class ptr_view final {
public:
  ptr_view() noexcept = default;

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
  friend ptr_view ptr_view_of(value::any_ref source) noexcept;

  ptr_view(const detail::ptr_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const detail::ptr_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

/** @brief 如果运行期引用具备对象指针语义，则创建对象指针视图。 */
[[nodiscard]] inline ptr_view ptr_view_of(value::any_ref source) noexcept {
  if (!source.valid()) {
    return {};
  }

  const meta::detail::type_descriptor* descriptor = value::detail::descriptor_of(source);
  const detail::ptr_data* data =
      meta::detail::descriptor_static_capability<detail::ptr_data>(descriptor);
  if (data == nullptr) {
    data = meta::capabilities::find<detail::ptr_data>(source.get_type().id());
  }
  if (data == nullptr) {
    return {};
  }

  return ptr_view{data, source.data(), source.writable()};
}

/** @brief 判断运行期引用是否具备对象指针语义。 */
[[nodiscard]] inline bool is_ptr(value::any_ref source) noexcept {
  return static_cast<bool>(ptr_view_of(source));
}

} // namespace caors::objects

#endif
