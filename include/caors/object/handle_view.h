// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_HANDLE_VIEW_H_
#define CAORS_OBJECT_HANDLE_VIEW_H_

#include "caors/meta/capability.h"
#include "caors/meta/detail/type_descriptor.h"
#include "caors/object/handle.h"
#include "caors/value/any_ref.h"

namespace caors::objects {

/**
 * @brief 对象身份句柄运行期视图。
 *
 * `handle_view` 不拥有对象，也不解析对象；它只把 `objects::handle` 和
 * `objects::typed_handle<T>` 统一视为可读写的对象身份。
 */
class handle_view final {
public:
  handle_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 获取当前保存的对象身份；空视图返回无效句柄。 */
  [[nodiscard]] handle get() const noexcept {
    if (!valid() || data_->get == nullptr) {
      return {};
    }

    return data_->get(storage_);
  }

  /** @brief 写入对象身份；只读或空视图时返回 `false`。 */
  bool set(handle value) const noexcept {
    if (!writable() || data_->set == nullptr) {
      return false;
    }

    return data_->set(const_cast<void*>(storage_), value);
  }

private:
  friend handle_view handle_view_of(value::any_ref source) noexcept;

  handle_view(const detail::handle_ops* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const detail::handle_ops* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

/** @brief 如果运行期引用具备对象身份句柄语义，则创建句柄视图。 */
[[nodiscard]] inline handle_view handle_view_of(value::any_ref source) noexcept {
  if (!source.valid()) {
    return {};
  }

  const meta::detail::type_descriptor* descriptor = value::detail::descriptor_of(source);
  const detail::handle_ops* data =
      meta::detail::descriptor_static_capability<detail::handle_ops>(descriptor);
  if (data == nullptr) {
    data = meta::capabilities::find<detail::handle_ops>(source.get_type().id());
  }
  if (data == nullptr) {
    return {};
  }

  return handle_view{data, source.data(), source.writable()};
}

/** @brief 判断运行期引用是否具备对象身份句柄语义。 */
[[nodiscard]] inline bool is_handle(value::any_ref source) noexcept {
  return static_cast<bool>(handle_view_of(source));
}

} // namespace caors::objects

#endif
