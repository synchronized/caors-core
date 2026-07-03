// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_ANY_REF_H_
#define CAORS_VALUE_ANY_REF_H_

#include "caors/value/any_storage.h"

namespace caors::value {

/**
 * @brief 运行期 C++ 类型引用。
 *
 * `any_ref` 不拥有对象，只保存外部对象地址和运行期类型信息。调用方必须保证被引用对象在
 * `any_ref` 使用期间仍然存活；可写引用只能从非 const 对象或可写字段创建。
 */
class any_ref final {
public:
  any_ref() noexcept = default;

  /** @brief 从可写 C++ 对象创建引用。 */
  template <typename T> [[nodiscard]] static any_ref from(T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(
        traits::has_type_name_v<type>,
        "any_ref::from<T>() requires a registered traits::type_name<T> or an explicit type name");
    return from(source, caors::traits::type_name<type>::value);
  }

  /** @brief 从可写 C++ 对象和显式类型名创建引用；类型名必须已经注册并匹配该 C++ 类型。 */
  template <typename T> [[nodiscard]] static any_ref from(T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const meta::type registered = caors::meta::find<type>();
    if (!registered || registered.name().empty() || registered.name() != name) {
      return {};
    }

    return from_raw(registered.id(), &source, !std::is_const_v<T>);
  }

  /** @brief 从只读 C++ 对象创建引用。 */
  template <typename T> [[nodiscard]] static any_ref from_const(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(traits::has_type_name_v<type>, "any_ref::from_const<T>() requires a registered "
                                                 "traits::type_name<T> or an explicit type name");
    return from_const(source, caors::traits::type_name<type>::value);
  }

  /** @brief 从只读 C++ 对象和显式类型名创建引用；类型名必须已经注册并匹配该 C++ 类型。 */
  template <typename T>
  [[nodiscard]] static any_ref from_const(const T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const meta::type registered = caors::meta::find<type>();
    if (!registered || registered.name().empty() || registered.name() != name) {
      return {};
    }

    return from_raw(registered.id(), &source, false);
  }

  [[nodiscard]] bool valid() const noexcept {
    const meta::type meta = caors::meta::find(type_);
    return meta.valid() && data_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id type() const noexcept { return type_; }
  [[nodiscard]] string_name type_name() const noexcept {
    const meta::type meta = caors::meta::find(type_);
    return meta ? meta.name() : string_name{};
  }
  [[nodiscard]] const void* data() const noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] void* mutable_data() const noexcept {
    return valid() && writable_ ? const_cast<void*>(data_) : nullptr;
  }
  [[nodiscard]] std::size_t size() const noexcept {
    const meta::type meta = caors::meta::find(type_);
    return meta ? meta.size() : 0;
  }
  [[nodiscard]] std::size_t alignment() const noexcept {
    const meta::type meta = caors::meta::find(type_);
    return meta ? meta.alignment() : 0;
  }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }
  [[nodiscard]] sequence_view create_sequence_view() const;
  [[nodiscard]] set_view create_set_view() const;
  [[nodiscard]] associative_view create_associative_view() const;
  [[nodiscard]] pair_view create_pair_view() const;
  [[nodiscard]] tuple_view create_tuple_view() const;
  [[nodiscard]] variant_view create_variant_view() const;
  [[nodiscard]] indirect_view create_indirect_view() const;

  /** @brief 判断当前引用是否匹配指定 C++ 类型。 */
  template <typename T> [[nodiscard]] bool matches_type() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    return valid() && type_ == meta::detail::type_id_of_cpp_type<type>() &&
           size() == sizeof(type) && alignment() == alignof(type);
  }

  /** @brief 判断两个运行期值是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any& other) const noexcept {
    return valid() && other.valid() && type_ == other.type() && size() == other.size() &&
           alignment() == other.alignment();
  }

  /** @brief 判断两个运行期引用是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any_ref& other) const noexcept {
    return valid() && other.valid() && type_ == other.type_ && size() == other.size() &&
           alignment() == other.alignment();
  }

  /** @brief 按指定类型获取只读指针。 */
  template <typename T>
  [[nodiscard]] const std::remove_cv_t<std::remove_reference_t<T>>* try_get() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>()) {
      return nullptr;
    }

    return static_cast<const type*>(data_);
  }

  /** @brief 按指定类型获取可写指针。 */
  template <typename T>
  [[nodiscard]] std::remove_cv_t<std::remove_reference_t<T>>* try_get_mutable() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>() || !writable_) {
      return nullptr;
    }

    return static_cast<type*>(const_cast<void*>(data_));
  }

  /** @brief 从同类型 C++ 值写入当前引用指向的对象。 */
  template <typename T> bool try_set(const T& source) const {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const meta::type meta = caors::meta::find(type_);
    if (!matches_type<type>() || !writable_ || !meta || meta.assign_value() == nullptr) {
      return false;
    }

    return meta.assign_value()(const_cast<void*>(data_), &source);
  }

  /** @brief 从同类型运行期值写入当前引用指向的对象。 */
  bool try_set(const any& source) const {
    const meta::type meta = caors::meta::find(type_);
    if (!matches_type(source) || !writable_ || !meta || meta.assign_value() == nullptr) {
      return false;
    }

    return meta.assign_value()(const_cast<void*>(data_), source.data());
  }

  /** @brief 从同类型运行期引用写入当前引用指向的对象。 */
  bool try_set(const any_ref& source) const {
    const meta::type meta = caors::meta::find(type_);
    if (!matches_type(source) || !writable_ || !meta || meta.assign_value() == nullptr) {
      return false;
    }

    return meta.assign_value()(const_cast<void*>(data_), source.data_);
  }

  /** @brief 尝试复制引用指向的对象。不可拷贝对象会返回无效 `any`。 */
  [[nodiscard]] any to_any() const { return any::copy_from_raw(type_, data_); }

private:
  friend class any;
  friend class indirect_view;
  friend class sequence_view;
  friend class set_view;
  friend class associative_view;
  friend class pair_view;
  friend class tuple_view;
  friend class variant_view;
  friend struct reflection::detail::field_data;

  [[nodiscard]] static any_ref from_raw(type_id type, const void* data, bool writable) noexcept {
    const meta::type meta = caors::meta::find(type);
    if (!meta.valid() || data == nullptr) {
      return {};
    }

    any_ref result;
    result.type_ = type;
    result.data_ = data;
    result.writable_ = writable;
    return result;
  }

  type_id type_{};
  const void* data_ = nullptr;
  bool writable_ = false;
};

inline any_ref any::ref() { return any_ref::from_raw(type_, data_, true); }

inline any_ref any::ref() const { return any_ref::from_raw(type_, data_, false); }

} // namespace caors::value

#endif
