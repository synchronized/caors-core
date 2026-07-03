// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_ANY_VALUE_H_
#define CAORS_VALUE_ANY_VALUE_H_

#include "caors/meta.h"
#include "caors/traits/container.h"

#include <cstddef>
#include <iterator>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::reflection {
struct field_data;
} // namespace caors::reflection

namespace caors::value {

class any_ref;
class indirect_view;
class sequence_view;
class set_view;
class associative_view;
class pair_view;
class tuple_view;
class variant_view;

using set_element_visitor = bool (*)(any_ref element, void* user_data);
using associative_entry_visitor = bool (*)(any_ref key, any_ref mapped, void* user_data);

/**
 * @brief 运行期 C++ 类型值。
 *
 * `any` 持有一份对象拷贝，并保存对应的运行期类型信息。它不引用原对象，因此可以安全地
 * 脱离字段所属对象生命周期使用。第一版只支持可拷贝类型；不可拷贝对象会产生无效 `any`。
 */
class any final {
public:
  any() noexcept = default;
  ~any() { reset(); }

  any(const any& other) { copy_from(other); }

  any(any&& other) noexcept { move_from(other); }

  any& operator=(const any& other) {
    if (this == &other) {
      return *this;
    }

    any copy{other};
    swap(copy);
    return *this;
  }

  any& operator=(any&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    reset();
    move_from(other);
    return *this;
  }

  /**
   * @brief 从 C++ 值创建运行期值。
   *
   * 类型必须已经注册到全局 meta 注册表。没有 `traits::type_name<T>`
   * 默认映射的类型需要使用显式类型名重载。
   */
  template <typename T> [[nodiscard]] static any from(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(
        detail::has_type_name_v<type>,
        "any::from<T>() requires a registered traits::type_name<T> or an explicit type name");
    return from(source, caors::traits::type_name<type>::value);
  }

  /** @brief 从 C++ 值和显式类型名创建运行期值；类型名必须已经注册并匹配该 C++ 类型。 */
  template <typename T> [[nodiscard]] static any from(const T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(std::is_copy_constructible_v<type>, "any requires copy constructible types");

    const std::optional<meta::type_view> registered = caors::meta::find<type>();
    if (!registered.has_value() || registered->name().empty() || registered->name() != name) {
      return {};
    }

    return copy_from_raw(registered->id(), &source);
  }

  /** @brief 按运行期类型 id 默认构造一个值。不可默认构造类型会返回无效 `any`。 */
  [[nodiscard]] static any default_construct(type_id type) {
    const std::optional<meta::type_view> meta = caors::meta::find(type);
    if (!meta.has_value() || !meta->valid() || meta->cpp_type() == nullptr ||
        meta->default_construct_value() == nullptr || meta->destroy_value() == nullptr) {
      return {};
    }

    void* value = meta->default_construct_value()();
    if (value == nullptr) {
      return {};
    }

    any result;
    result.type_ = type;
    result.data_ = value;
    return result;
  }

  [[nodiscard]] bool valid() const noexcept {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    return meta.has_value() && meta->valid() && meta->cpp_type() != nullptr && data_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id type() const noexcept { return type_; }
  [[nodiscard]] string_name type_name() const noexcept {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    return meta.has_value() ? meta->name() : string_name{};
  }
  [[nodiscard]] detail::cpp_type_key cpp_type() const noexcept {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    return meta.has_value() ? meta->cpp_type() : nullptr;
  }
  [[nodiscard]] const void* data() const noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] void* mutable_data() noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] any_ref ref();
  [[nodiscard]] any_ref ref() const;
  [[nodiscard]] std::size_t size() const noexcept {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    return meta.has_value() ? meta->size() : 0;
  }
  [[nodiscard]] std::size_t alignment() const noexcept {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    return meta.has_value() ? meta->alignment() : 0;
  }
  [[nodiscard]] sequence_view create_sequence_view();
  [[nodiscard]] sequence_view create_sequence_view() const;
  [[nodiscard]] set_view create_set_view();
  [[nodiscard]] set_view create_set_view() const;
  [[nodiscard]] associative_view create_associative_view();
  [[nodiscard]] associative_view create_associative_view() const;
  [[nodiscard]] pair_view create_pair_view();
  [[nodiscard]] pair_view create_pair_view() const;
  [[nodiscard]] tuple_view create_tuple_view();
  [[nodiscard]] tuple_view create_tuple_view() const;
  [[nodiscard]] variant_view create_variant_view();
  [[nodiscard]] variant_view create_variant_view() const;
  [[nodiscard]] indirect_view create_indirect_view();
  [[nodiscard]] indirect_view create_indirect_view() const;

  /** @brief 清空当前值。 */
  void reset() noexcept {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    if (data_ != nullptr && meta.has_value() && meta->destroy_value() != nullptr) {
      meta->destroy_value()(data_);
    }

    type_ = {};
    data_ = nullptr;
  }

  /** @brief 交换两个运行期值。 */
  void swap(any& other) noexcept {
    using std::swap;
    swap(type_, other.type_);
    swap(data_, other.data_);
  }

  /** @brief 判断当前值是否匹配指定 C++ 类型。 */
  template <typename T> [[nodiscard]] bool matches_type() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    return valid() && cpp_type() == detail::cpp_type_key_of<type>() && size() == sizeof(type) &&
           alignment() == alignof(type);
  }

  /** @brief 判断两个运行期值是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any& other) const noexcept {
    return valid() && other.valid() && cpp_type() == other.cpp_type() && size() == other.size() &&
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
  [[nodiscard]] std::remove_cv_t<std::remove_reference_t<T>>* try_get_mutable() noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>()) {
      return nullptr;
    }

    return static_cast<type*>(data_);
  }

  /** @brief 从同类型 C++ 值写入当前值。 */
  template <typename T> bool try_set(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    if (!matches_type<type>() || !meta.has_value() || meta->assign_value() == nullptr) {
      return false;
    }

    return meta->assign_value()(data_, &source);
  }

  /** @brief 从同类型运行期值写入当前值。 */
  bool try_set(const any& source) {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    if (!matches_type(source) || !meta.has_value() || meta->assign_value() == nullptr) {
      return false;
    }

    return meta->assign_value()(data_, source.data_);
  }

private:
  friend class any_ref;
  friend struct reflection::field_data;

  [[nodiscard]] static any copy_from_raw(type_id type, const void* source) {
    const std::optional<meta::type_view> meta = caors::meta::find(type);
    if (!meta.has_value() || !meta->valid() || meta->cpp_type() == nullptr || source == nullptr ||
        meta->clone_value() == nullptr || meta->destroy_value() == nullptr) {
      return {};
    }

    void* copy = meta->clone_value()(source);
    if (copy == nullptr) {
      return {};
    }

    any result;
    result.type_ = type;
    result.data_ = copy;
    return result;
  }

  void copy_from(const any& other) {
    if (!other.valid()) {
      return;
    }

    *this = copy_from_raw(other.type_, other.data_);
  }

  void move_from(any& other) noexcept {
    type_ = other.type_;
    data_ = other.data_;

    other.type_ = {};
    other.data_ = nullptr;
  }

  type_id type_{};
  void* data_ = nullptr;
};

inline void swap(any& lhs, any& rhs) noexcept { lhs.swap(rhs); }

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
        detail::has_type_name_v<type>,
        "any_ref::from<T>() requires a registered traits::type_name<T> or an explicit type name");
    return from(source, caors::traits::type_name<type>::value);
  }

  /** @brief 从可写 C++ 对象和显式类型名创建引用；类型名必须已经注册并匹配该 C++ 类型。 */
  template <typename T> [[nodiscard]] static any_ref from(T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const std::optional<meta::type_view> registered = caors::meta::find<type>();
    if (!registered.has_value() || registered->name().empty() || registered->name() != name) {
      return {};
    }

    return from_raw(registered->id(), &source, !std::is_const_v<T>);
  }

  /** @brief 从只读 C++ 对象创建引用。 */
  template <typename T> [[nodiscard]] static any_ref from_const(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(detail::has_type_name_v<type>, "any_ref::from_const<T>() requires a registered "
                                                 "traits::type_name<T> or an explicit type name");
    return from_const(source, caors::traits::type_name<type>::value);
  }

  /** @brief 从只读 C++ 对象和显式类型名创建引用；类型名必须已经注册并匹配该 C++ 类型。 */
  template <typename T>
  [[nodiscard]] static any_ref from_const(const T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const std::optional<meta::type_view> registered = caors::meta::find<type>();
    if (!registered.has_value() || registered->name().empty() || registered->name() != name) {
      return {};
    }

    return from_raw(registered->id(), &source, false);
  }

  [[nodiscard]] bool valid() const noexcept {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    return meta.has_value() && meta->valid() && meta->cpp_type() != nullptr && data_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id type() const noexcept { return type_; }
  [[nodiscard]] string_name type_name() const noexcept {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    return meta.has_value() ? meta->name() : string_name{};
  }
  [[nodiscard]] detail::cpp_type_key cpp_type() const noexcept {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    return meta.has_value() ? meta->cpp_type() : nullptr;
  }
  [[nodiscard]] const void* data() const noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] void* mutable_data() const noexcept {
    return valid() && writable_ ? const_cast<void*>(data_) : nullptr;
  }
  [[nodiscard]] std::size_t size() const noexcept {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    return meta.has_value() ? meta->size() : 0;
  }
  [[nodiscard]] std::size_t alignment() const noexcept {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    return meta.has_value() ? meta->alignment() : 0;
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
    return valid() && cpp_type() == detail::cpp_type_key_of<type>() && size() == sizeof(type) &&
           alignment() == alignof(type);
  }

  /** @brief 判断两个运行期值是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any& other) const noexcept {
    return valid() && other.valid() && cpp_type() == other.cpp_type() && size() == other.size() &&
           alignment() == other.alignment();
  }

  /** @brief 判断两个运行期引用是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any_ref& other) const noexcept {
    return valid() && other.valid() && cpp_type() == other.cpp_type() && size() == other.size() &&
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
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    if (!matches_type<type>() || !writable_ || !meta.has_value() ||
        meta->assign_value() == nullptr) {
      return false;
    }

    return meta->assign_value()(const_cast<void*>(data_), &source);
  }

  /** @brief 从同类型运行期值写入当前引用指向的对象。 */
  bool try_set(const any& source) const {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    if (!matches_type(source) || !writable_ || !meta.has_value() ||
        meta->assign_value() == nullptr) {
      return false;
    }

    return meta->assign_value()(const_cast<void*>(data_), source.data());
  }

  /** @brief 从同类型运行期引用写入当前引用指向的对象。 */
  bool try_set(const any_ref& source) const {
    const std::optional<meta::type_view> meta = caors::meta::find(type_);
    if (!matches_type(source) || !writable_ || !meta.has_value() ||
        meta->assign_value() == nullptr) {
      return false;
    }

    return meta->assign_value()(const_cast<void*>(data_), source.data_);
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
  friend struct reflection::field_data;

  [[nodiscard]] static any_ref from_raw(type_id type, const void* data, bool writable) noexcept {
    const std::optional<meta::type_view> meta = caors::meta::find(type);
    if (!meta.has_value() || !meta->valid() || meta->cpp_type() == nullptr || data == nullptr) {
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
