// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_ANY_STORAGE_H_
#define CAORS_VALUE_ANY_STORAGE_H_

#include "caors/meta.h"
#include "caors/traits/container.h"
#include "caors/traits/type_name.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::reflection::detail {
struct field_data;
} // namespace caors::reflection::detail

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
        traits::has_type_name_v<type>,
        "any::from<T>() requires a registered traits::type_name<T> or an explicit type name");
    return from(source, caors::traits::type_name<type>::value);
  }

  /** @brief 从 C++ 值和显式类型名创建运行期值；类型名必须已经注册并匹配该 C++ 类型。 */
  template <typename T> [[nodiscard]] static any from(const T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(std::is_copy_constructible_v<type>, "any requires copy constructible types");

    const meta::type_view registered = caors::meta::find<type>();
    if (!registered || registered.name().empty() || registered.name() != name) {
      return {};
    }

    return copy_from_raw(registered.id(), &source);
  }

  /** @brief 按运行期类型 id 默认构造一个值。不可默认构造类型会返回无效 `any`。 */
  [[nodiscard]] static any default_construct(type_id type) {
    const meta::type_view meta = caors::meta::find(type);
    if (!meta.valid() || meta.default_construct_value() == nullptr ||
        meta.destroy_value() == nullptr) {
      return {};
    }

    void* value = meta.default_construct_value()();
    if (value == nullptr) {
      return {};
    }

    any result;
    result.type_ = type;
    result.data_ = value;
    return result;
  }

  [[nodiscard]] bool valid() const noexcept {
    const meta::type_view meta = caors::meta::find(type_);
    return meta.valid() && data_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id type() const noexcept { return type_; }
  [[nodiscard]] string_name type_name() const noexcept {
    const meta::type_view meta = caors::meta::find(type_);
    return meta ? meta.name() : string_name{};
  }
  [[nodiscard]] const void* data() const noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] void* mutable_data() noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] any_ref ref();
  [[nodiscard]] any_ref ref() const;
  [[nodiscard]] std::size_t size() const noexcept {
    const meta::type_view meta = caors::meta::find(type_);
    return meta ? meta.size() : 0;
  }
  [[nodiscard]] std::size_t alignment() const noexcept {
    const meta::type_view meta = caors::meta::find(type_);
    return meta ? meta.alignment() : 0;
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
    const meta::type_view meta = caors::meta::find(type_);
    if (data_ != nullptr && meta && meta.destroy_value() != nullptr) {
      meta.destroy_value()(data_);
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
    return valid() && type_ == meta::detail::type_id_of_cpp_type<type>() &&
           size() == sizeof(type) && alignment() == alignof(type);
  }

  /** @brief 判断两个运行期值是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any& other) const noexcept {
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
    const meta::type_view meta = caors::meta::find(type_);
    if (!matches_type<type>() || !meta || meta.assign_value() == nullptr) {
      return false;
    }

    return meta.assign_value()(data_, &source);
  }

  /** @brief 从同类型运行期值写入当前值。 */
  bool try_set(const any& source) {
    const meta::type_view meta = caors::meta::find(type_);
    if (!matches_type(source) || !meta || meta.assign_value() == nullptr) {
      return false;
    }

    return meta.assign_value()(data_, source.data_);
  }

private:
  friend class any_ref;
  friend struct reflection::detail::field_data;

  [[nodiscard]] static any copy_from_raw(type_id type, const void* source) {
    const meta::type_view meta = caors::meta::find(type);
    if (!meta.valid() || source == nullptr || meta.clone_value() == nullptr ||
        meta.destroy_value() == nullptr) {
      return {};
    }

    void* copy = meta.clone_value()(source);
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

} // namespace caors::value

#endif
