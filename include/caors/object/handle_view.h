// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_HANDLE_VIEW_H_
#define CAORS_OBJECT_HANDLE_VIEW_H_

#include "caors/meta/value/any_ref.h"
#include "caors/object/handle.h"

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
  friend handle_view handle_view_of(meta::any_ref source) noexcept;

  handle_view(const handle_ops* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const handle_ops* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

/** @brief 如果运行期引用具备对象身份句柄语义，则创建句柄视图。 */
[[nodiscard]] inline handle_view handle_view_of(meta::any_ref source) noexcept {
  if (!source.valid()) {
    return {};
  }

  const handle_ops* data = source.get_type().get_capability<handle_ops>();
  if (data == nullptr) {
    return {};
  }

  return handle_view{data, source.data(), source.writable()};
}

/** @brief 判断运行期引用是否具备对象身份句柄语义。 */
[[nodiscard]] inline bool is_handle(meta::any_ref source) noexcept {
  return static_cast<bool>(handle_view_of(source));
}

} // namespace caors::objects

namespace caors::meta {

template <> struct capability_view_traits<objects::handle_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static objects::handle_view bind(meta::any_ref source) noexcept {
    return objects::handle_view_of(source);
  }
};

} // namespace caors::meta

#endif
