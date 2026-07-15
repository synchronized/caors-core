// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_PTR_VIEW_H_
#define CAORS_OBJECT_PTR_VIEW_H_

#include "caors/meta/value/any_ref.h"
#include "caors/object/ptr.h"

namespace caors::objects {

class ptr_view;

/** @brief 如果运行期引用具备对象指针语义，则创建对象指针视图。 */
[[nodiscard]] ptr_view make_ptr_view(meta::any_ref source) noexcept;

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
  friend ptr_view make_ptr_view(meta::any_ref source) noexcept;

  ptr_view(const ptr_ops* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const ptr_ops* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

/** @brief 对象指针类型级能力 facade。 */
class ptr_capability final {
public:
  ptr_capability() noexcept = default;
  explicit ptr_capability(meta::type type) noexcept
      : type_(type), ops_(type.get_raw_ops<ptr_ops>()) {}

  [[nodiscard]] bool valid() const noexcept { return type_ && ops_ != nullptr && ops_->valid(); }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }

  [[nodiscard]] ptr_view bind(meta::any_ref source) const noexcept {
    if (!valid() || !source.valid() || source.get_type().id() != type_.id()) {
      return {};
    }

    return make_ptr_view(source);
  }

private:
  meta::type type_{};
  const ptr_ops* ops_ = nullptr;
};

[[nodiscard]] inline ptr_view make_ptr_view(meta::any_ref source) noexcept {
  if (!source.valid()) {
    return {};
  }

  const ptr_ops* data = source.get_type().get_raw_ops<ptr_ops>();
  if (data == nullptr) {
    return {};
  }

  return ptr_view{data, source.data(), source.writable()};
}

/** @brief 判断运行期引用是否具备对象指针语义。 */
[[nodiscard]] inline bool is_ptr(meta::any_ref source) noexcept {
  return static_cast<bool>(make_ptr_view(source));
}

} // namespace caors::objects

namespace caors::meta {

template <> struct capability_view_traits<objects::ptr_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static objects::ptr_view bind(meta::any_ref source) noexcept {
    return objects::make_ptr_view(source);
  }
};

template <> struct type_capability_traits<objects::ptr_capability> {
  using result_type = objects::ptr_capability;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return objects::ptr_capability{source};
  }

  [[nodiscard]] static bool has(const type& source) noexcept {
    return objects::ptr_capability{source}.valid();
  }
};

} // namespace caors::meta

#endif
