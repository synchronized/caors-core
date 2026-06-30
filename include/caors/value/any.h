// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_ANY_H_
#define CAORS_VALUE_ANY_H_

#include "caors/meta.h"

#include <cstddef>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors {

namespace detail {

using value_clone_fn = void* (*)(const void*);
using value_destroy_fn = void (*)(void*) noexcept;
using value_assign_fn = bool (*)(void*, const void*);

template <typename T> void* clone_value(const void* source) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_copy_constructible_v<type>) {
    return new type(*static_cast<const type*>(source));
  } else {
    return nullptr;
  }
}

template <typename T> void destroy_value(void* value) noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  delete static_cast<type*>(value);
}

template <typename T> bool assign_value(void* target, const void* source) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_assignable_v<type&, const type&>) {
    *static_cast<type*>(target) = *static_cast<const type*>(source);
    return true;
  } else {
    return false;
  }
}

template <typename T> [[nodiscard]] value_clone_fn value_clone_fn_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_copy_constructible_v<type>) {
    return &clone_value<type>;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] value_destroy_fn value_destroy_fn_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_copy_constructible_v<type>) {
    return &destroy_value<type>;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] value_assign_fn value_assign_fn_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_assignable_v<type&, const type&>) {
    return &assign_value<type>;
  } else {
    return nullptr;
  }
}

} // namespace detail

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
   * 没有 `type_name<T>` 默认映射的类型需要使用显式类型名重载。
   */
  template <typename T> [[nodiscard]] static any from(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(detail::has_type_name_v<type>,
                  "any::from<T>() requires type_name<T> or an explicit type name");
    return from(source, caors::type_name<type>::value);
  }

  /** @brief 从 C++ 值和显式类型名创建运行期值。 */
  template <typename T> [[nodiscard]] static any from(const T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(std::is_copy_constructible_v<type>, "any requires copy constructible types");

    const error result = caors::register_type<type>(name);
    if (result && result.value() != errc::duplicate_id) {
      return {};
    }

    const std::optional<meta_type> registered = caors::find_type<type>();
    if (!registered.has_value() || registered->name().empty()) {
      return {};
    }

    return copy_from_raw(registered->id(), registered->name(), detail::cpp_type_key_of<type>(),
                         &source, sizeof(type), alignof(type), detail::value_clone_fn_of<type>(),
                         detail::value_destroy_fn_of<type>(), detail::value_assign_fn_of<type>());
  }

  [[nodiscard]] bool valid() const noexcept {
    return static_cast<bool>(type_) && !type_name_.empty() && cpp_type_ != nullptr &&
           data_ != nullptr && size_ > 0 && alignment_ > 0 && clone_ != nullptr &&
           destroy_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id type() const noexcept { return type_; }
  [[nodiscard]] string_name type_name() const noexcept { return type_name_; }
  [[nodiscard]] detail::cpp_type_key cpp_type() const noexcept { return cpp_type_; }
  [[nodiscard]] const void* data() const noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] void* mutable_data() noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::size_t alignment() const noexcept { return alignment_; }

  /** @brief 清空当前值。 */
  void reset() noexcept {
    if (data_ != nullptr && destroy_ != nullptr) {
      destroy_(data_);
    }

    type_ = {};
    type_name_ = {};
    cpp_type_ = nullptr;
    data_ = nullptr;
    size_ = 0;
    alignment_ = 0;
    clone_ = nullptr;
    destroy_ = nullptr;
    assign_ = nullptr;
  }

  /** @brief 交换两个运行期值。 */
  void swap(any& other) noexcept {
    using std::swap;
    swap(type_, other.type_);
    swap(type_name_, other.type_name_);
    swap(cpp_type_, other.cpp_type_);
    swap(data_, other.data_);
    swap(size_, other.size_);
    swap(alignment_, other.alignment_);
    swap(clone_, other.clone_);
    swap(destroy_, other.destroy_);
    swap(assign_, other.assign_);
  }

  /** @brief 判断当前值是否匹配指定 C++ 类型。 */
  template <typename T> [[nodiscard]] bool matches_type() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    return valid() && cpp_type_ == detail::cpp_type_key_of<type>() && size_ == sizeof(type) &&
           alignment_ == alignof(type);
  }

  /** @brief 判断两个运行期值是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any& other) const noexcept {
    return valid() && other.valid() && cpp_type_ == other.cpp_type_ && size_ == other.size_ &&
           alignment_ == other.alignment_;
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
    if (!matches_type<type>() || assign_ == nullptr) {
      return false;
    }

    return assign_(data_, &source);
  }

  /** @brief 从同类型运行期值写入当前值。 */
  bool try_set(const any& source) {
    if (!matches_type(source) || assign_ == nullptr) {
      return false;
    }

    return assign_(data_, source.data_);
  }

private:
  friend class any_ref;
  friend struct reflection_field_data;

  [[nodiscard]] static any copy_from_raw(type_id type, string_name type_name,
                                         detail::cpp_type_key cpp_type, const void* source,
                                         std::size_t size, std::size_t alignment,
                                         detail::value_clone_fn clone,
                                         detail::value_destroy_fn destroy,
                                         detail::value_assign_fn assign) {
    if (!type || type_name.empty() || cpp_type == nullptr || source == nullptr || size == 0 ||
        alignment == 0 || clone == nullptr || destroy == nullptr) {
      return {};
    }

    void* copy = clone(source);
    if (copy == nullptr) {
      return {};
    }

    any result;
    result.type_ = type;
    result.type_name_ = type_name;
    result.cpp_type_ = cpp_type;
    result.data_ = copy;
    result.size_ = size;
    result.alignment_ = alignment;
    result.clone_ = clone;
    result.destroy_ = destroy;
    result.assign_ = assign;
    return result;
  }

  void copy_from(const any& other) {
    if (!other.valid()) {
      return;
    }

    *this = copy_from_raw(other.type_, other.type_name_, other.cpp_type_, other.data_, other.size_,
                          other.alignment_, other.clone_, other.destroy_, other.assign_);
  }

  void move_from(any& other) noexcept {
    type_ = other.type_;
    type_name_ = other.type_name_;
    cpp_type_ = other.cpp_type_;
    data_ = other.data_;
    size_ = other.size_;
    alignment_ = other.alignment_;
    clone_ = other.clone_;
    destroy_ = other.destroy_;
    assign_ = other.assign_;

    other.type_ = {};
    other.type_name_ = {};
    other.cpp_type_ = nullptr;
    other.data_ = nullptr;
    other.size_ = 0;
    other.alignment_ = 0;
    other.clone_ = nullptr;
    other.destroy_ = nullptr;
    other.assign_ = nullptr;
  }

  type_id type_{};
  string_name type_name_{};
  detail::cpp_type_key cpp_type_ = nullptr;
  void* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t alignment_ = 0;
  detail::value_clone_fn clone_ = nullptr;
  detail::value_destroy_fn destroy_ = nullptr;
  detail::value_assign_fn assign_ = nullptr;
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
    static_assert(detail::has_type_name_v<type>,
                  "any_ref::from<T>() requires type_name<T> or an explicit type name");
    return from(source, caors::type_name<type>::value);
  }

  /** @brief 从可写 C++ 对象和显式类型名创建引用。 */
  template <typename T> [[nodiscard]] static any_ref from(T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const error result = caors::register_type<type>(name);
    if (result && result.value() != errc::duplicate_id) {
      return {};
    }

    const std::optional<meta_type> registered = caors::find_type<type>();
    if (!registered.has_value() || registered->name().empty()) {
      return {};
    }

    return from_raw(registered->id(), registered->name(), detail::cpp_type_key_of<type>(), &source,
                    sizeof(type), alignof(type), detail::value_clone_fn_of<type>(),
                    detail::value_destroy_fn_of<type>(), detail::value_assign_fn_of<type>(),
                    !std::is_const_v<T>);
  }

  /** @brief 从只读 C++ 对象创建引用。 */
  template <typename T> [[nodiscard]] static any_ref from_const(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(detail::has_type_name_v<type>,
                  "any_ref::from_const<T>() requires type_name<T> or an explicit type name");
    return from_const(source, caors::type_name<type>::value);
  }

  /** @brief 从只读 C++ 对象和显式类型名创建引用。 */
  template <typename T>
  [[nodiscard]] static any_ref from_const(const T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const error result = caors::register_type<type>(name);
    if (result && result.value() != errc::duplicate_id) {
      return {};
    }

    const std::optional<meta_type> registered = caors::find_type<type>();
    if (!registered.has_value() || registered->name().empty()) {
      return {};
    }

    return from_raw(registered->id(), registered->name(), detail::cpp_type_key_of<type>(), &source,
                    sizeof(type), alignof(type), detail::value_clone_fn_of<type>(),
                    detail::value_destroy_fn_of<type>(), detail::value_assign_fn_of<type>(), false);
  }

  [[nodiscard]] bool valid() const noexcept {
    return static_cast<bool>(type_) && !type_name_.empty() && cpp_type_ != nullptr &&
           data_ != nullptr && size_ > 0 && alignment_ > 0;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id type() const noexcept { return type_; }
  [[nodiscard]] string_name type_name() const noexcept { return type_name_; }
  [[nodiscard]] detail::cpp_type_key cpp_type() const noexcept { return cpp_type_; }
  [[nodiscard]] const void* data() const noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] void* mutable_data() const noexcept {
    return valid() && writable_ ? const_cast<void*>(data_) : nullptr;
  }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::size_t alignment() const noexcept { return alignment_; }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 判断当前引用是否匹配指定 C++ 类型。 */
  template <typename T> [[nodiscard]] bool matches_type() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    return valid() && cpp_type_ == detail::cpp_type_key_of<type>() && size_ == sizeof(type) &&
           alignment_ == alignof(type);
  }

  /** @brief 判断两个运行期值是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any& other) const noexcept {
    return valid() && other.valid() && cpp_type_ == other.cpp_type() && size_ == other.size() &&
           alignment_ == other.alignment();
  }

  /** @brief 判断两个运行期引用是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any_ref& other) const noexcept {
    return valid() && other.valid() && cpp_type_ == other.cpp_type_ && size_ == other.size_ &&
           alignment_ == other.alignment_;
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
    if (!matches_type<type>() || !writable_ || assign_ == nullptr) {
      return false;
    }

    return assign_(const_cast<void*>(data_), &source);
  }

  /** @brief 从同类型运行期值写入当前引用指向的对象。 */
  bool try_set(const any& source) const {
    if (!matches_type(source) || !writable_ || assign_ == nullptr) {
      return false;
    }

    return assign_(const_cast<void*>(data_), source.data());
  }

  /** @brief 从同类型运行期引用写入当前引用指向的对象。 */
  bool try_set(const any_ref& source) const {
    if (!matches_type(source) || !writable_ || assign_ == nullptr) {
      return false;
    }

    return assign_(const_cast<void*>(data_), source.data_);
  }

  /** @brief 尝试复制引用指向的对象。不可拷贝对象会返回无效 `any`。 */
  [[nodiscard]] any to_any() const {
    return any::copy_from_raw(type_, type_name_, cpp_type_, data_, size_, alignment_, clone_,
                              destroy_, assign_);
  }

private:
  friend struct reflection_field_data;

  [[nodiscard]] static any_ref from_raw(type_id type, string_name type_name,
                                        detail::cpp_type_key cpp_type, const void* data,
                                        std::size_t size, std::size_t alignment,
                                        detail::value_clone_fn clone,
                                        detail::value_destroy_fn destroy,
                                        detail::value_assign_fn assign, bool writable) noexcept {
    if (!type || type_name.empty() || cpp_type == nullptr || data == nullptr || size == 0 ||
        alignment == 0) {
      return {};
    }

    any_ref result;
    result.type_ = type;
    result.type_name_ = type_name;
    result.cpp_type_ = cpp_type;
    result.data_ = data;
    result.size_ = size;
    result.alignment_ = alignment;
    result.clone_ = clone;
    result.destroy_ = destroy;
    result.assign_ = assign;
    result.writable_ = writable;
    return result;
  }

  type_id type_{};
  string_name type_name_{};
  detail::cpp_type_key cpp_type_ = nullptr;
  const void* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t alignment_ = 0;
  detail::value_clone_fn clone_ = nullptr;
  detail::value_destroy_fn destroy_ = nullptr;
  detail::value_assign_fn assign_ = nullptr;
  bool writable_ = false;
};

} // namespace caors

#endif
