// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_ANY_H_
#define CAORS_VALUE_ANY_H_

#include "caors/meta.h"
#include "caors/meta/container_traits.h"

#include <cstddef>
#include <iterator>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors {

class any_ref;
class indirect_view;
class sequence_view;
class set_view;
class associative_view;
class pair_view;
class tuple_view;

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

    (void)detail::register_container_data_for<type>(registered->id());

    return copy_from_raw(registered->id(), &source);
  }

  /** @brief 按运行期类型 id 默认构造一个值。不可默认构造类型会返回无效 `any`。 */
  [[nodiscard]] static any default_construct(type_id type) {
    const std::optional<meta_type> meta = caors::find_type(type);
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
    const std::optional<meta_type> meta = caors::find_type(type_);
    return meta.has_value() && meta->valid() && meta->cpp_type() != nullptr && data_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id type() const noexcept { return type_; }
  [[nodiscard]] string_name type_name() const noexcept {
    const std::optional<meta_type> meta = caors::find_type(type_);
    return meta.has_value() ? meta->name() : string_name{};
  }
  [[nodiscard]] detail::cpp_type_key cpp_type() const noexcept {
    const std::optional<meta_type> meta = caors::find_type(type_);
    return meta.has_value() ? meta->cpp_type() : nullptr;
  }
  [[nodiscard]] const void* data() const noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] void* mutable_data() noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] any_ref ref();
  [[nodiscard]] any_ref ref() const;
  [[nodiscard]] std::size_t size() const noexcept {
    const std::optional<meta_type> meta = caors::find_type(type_);
    return meta.has_value() ? meta->size() : 0;
  }
  [[nodiscard]] std::size_t alignment() const noexcept {
    const std::optional<meta_type> meta = caors::find_type(type_);
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
  [[nodiscard]] indirect_view create_indirect_view();
  [[nodiscard]] indirect_view create_indirect_view() const;

  /** @brief 清空当前值。 */
  void reset() noexcept {
    const std::optional<meta_type> meta = caors::find_type(type_);
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
    const std::optional<meta_type> meta = caors::find_type(type_);
    if (!matches_type<type>() || !meta.has_value() || meta->assign_value() == nullptr) {
      return false;
    }

    return meta->assign_value()(data_, &source);
  }

  /** @brief 从同类型运行期值写入当前值。 */
  bool try_set(const any& source) {
    const std::optional<meta_type> meta = caors::find_type(type_);
    if (!matches_type(source) || !meta.has_value() || meta->assign_value() == nullptr) {
      return false;
    }

    return meta->assign_value()(data_, source.data_);
  }

private:
  friend class any_ref;
  friend struct reflection_field_data;

  [[nodiscard]] static any copy_from_raw(type_id type, const void* source) {
    const std::optional<meta_type> meta = caors::find_type(type);
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

    (void)detail::register_container_data_for<type>(registered->id());

    return from_raw(registered->id(), &source, !std::is_const_v<T>);
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

    (void)detail::register_container_data_for<type>(registered->id());

    return from_raw(registered->id(), &source, false);
  }

  [[nodiscard]] bool valid() const noexcept {
    const std::optional<meta_type> meta = caors::find_type(type_);
    return meta.has_value() && meta->valid() && meta->cpp_type() != nullptr && data_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id type() const noexcept { return type_; }
  [[nodiscard]] string_name type_name() const noexcept {
    const std::optional<meta_type> meta = caors::find_type(type_);
    return meta.has_value() ? meta->name() : string_name{};
  }
  [[nodiscard]] detail::cpp_type_key cpp_type() const noexcept {
    const std::optional<meta_type> meta = caors::find_type(type_);
    return meta.has_value() ? meta->cpp_type() : nullptr;
  }
  [[nodiscard]] const void* data() const noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] void* mutable_data() const noexcept {
    return valid() && writable_ ? const_cast<void*>(data_) : nullptr;
  }
  [[nodiscard]] std::size_t size() const noexcept {
    const std::optional<meta_type> meta = caors::find_type(type_);
    return meta.has_value() ? meta->size() : 0;
  }
  [[nodiscard]] std::size_t alignment() const noexcept {
    const std::optional<meta_type> meta = caors::find_type(type_);
    return meta.has_value() ? meta->alignment() : 0;
  }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }
  [[nodiscard]] sequence_view create_sequence_view() const;
  [[nodiscard]] set_view create_set_view() const;
  [[nodiscard]] associative_view create_associative_view() const;
  [[nodiscard]] pair_view create_pair_view() const;
  [[nodiscard]] tuple_view create_tuple_view() const;
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
    const std::optional<meta_type> meta = caors::find_type(type_);
    if (!matches_type<type>() || !writable_ || !meta.has_value() ||
        meta->assign_value() == nullptr) {
      return false;
    }

    return meta->assign_value()(const_cast<void*>(data_), &source);
  }

  /** @brief 从同类型运行期值写入当前引用指向的对象。 */
  bool try_set(const any& source) const {
    const std::optional<meta_type> meta = caors::find_type(type_);
    if (!matches_type(source) || !writable_ || !meta.has_value() ||
        meta->assign_value() == nullptr) {
      return false;
    }

    return meta->assign_value()(const_cast<void*>(data_), source.data());
  }

  /** @brief 从同类型运行期引用写入当前引用指向的对象。 */
  bool try_set(const any_ref& source) const {
    const std::optional<meta_type> meta = caors::find_type(type_);
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
  friend struct reflection_field_data;

  [[nodiscard]] static any_ref from_raw(type_id type, const void* data, bool writable) noexcept {
    const std::optional<meta_type> meta = caors::find_type(type);
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

/**
 * @brief 指针和 wrapper 的内部值视图。
 *
 * `indirect_view` 不拥有 wrapper 或内部值，只引用 `any` / `any_ref` 指向的 wrapper 对象。
 * 可写视图才允许重置 wrapper 或返回可写内部值引用。
 */
class indirect_view final {
public:
  indirect_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    if (data_ == nullptr || storage_ == nullptr || !data_->value_type) {
      return false;
    }

    if (data_->kind != type_kind::raw_pointer && data_->kind != type_kind::wrapper) {
      return false;
    }

    return data_->indirect_value != nullptr || data_->mutable_indirect_value != nullptr ||
           data_->has_indirect_value != nullptr || data_->reset_indirect_value != nullptr ||
           data_->ensure_indirect_value != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }
  [[nodiscard]] type_id value_type() const noexcept {
    return valid() ? data_->value_type : type_id{};
  }

  [[nodiscard]] string_name value_type_name() const noexcept {
    const std::optional<meta_type> meta = valid() ? find_type(data_->value_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 判断 wrapper 当前是否持有内部值。 */
  [[nodiscard]] bool has_value() const noexcept {
    if (!valid() || data_->has_indirect_value == nullptr) {
      return false;
    }

    return data_->has_indirect_value(storage_);
  }

  /** @brief 获取内部值引用。 */
  [[nodiscard]] any_ref value_ref() const {
    if (!valid() || data_->indirect_value == nullptr) {
      return {};
    }

    const void* value = nullptr;
    bool value_writable = false;
    if (writable() && data_->mutable_indirect_value != nullptr) {
      value = data_->mutable_indirect_value(const_cast<void*>(storage_));
      value_writable = value != nullptr;
    } else {
      value = data_->indirect_value(storage_);
    }

    if (value == nullptr) {
      return {};
    }

    return any_ref::from_raw(data_->value_type, value, value_writable);
  }

  /** @brief 重置 wrapper 或裸指针为空。 */
  bool reset() const {
    if (!writable() || data_->reset_indirect_value == nullptr) {
      return false;
    }

    return data_->reset_indirect_value(const_cast<void*>(storage_));
  }

  /** @brief 确保 wrapper 持有内部值。裸指针不会自动分配内存。 */
  bool ensure_value() const {
    if (!writable() || data_->ensure_indirect_value == nullptr) {
      return false;
    }

    return data_->ensure_indirect_value(const_cast<void*>(storage_));
  }

private:
  friend class any;
  friend class any_ref;

  indirect_view(const meta_type_semantics_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const meta_type_semantics_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

/** @brief 关联容器 key/value 引用。 */
class associative_entry_ref final {
public:
  associative_entry_ref() noexcept = default;
  associative_entry_ref(any_ref key, any_ref value) noexcept : key_(key), value_(value) {}

  [[nodiscard]] any_ref key() const noexcept { return key_; }
  [[nodiscard]] any_ref value() const noexcept { return value_; }
  [[nodiscard]] any_ref mapped() const noexcept { return value_; }

private:
  any_ref key_{};
  any_ref value_{};
};

/**
 * @brief 二元组运行期视图。
 *
 * `pair_view` 不拥有二元组，只引用 `any` 或 `any_ref` 指向的 pair 对象。first 通常用于 key
 * 语义，second 作为统一 value 语义暴露。
 */
class pair_view final {
public:
  pair_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询 first 类型。 */
  [[nodiscard]] type_id first_type() const noexcept {
    return valid() ? data_->key_type : type_id{};
  }

  /** @brief 查询 first 类型名。 */
  [[nodiscard]] string_name first_type_name() const noexcept {
    const std::optional<meta_type> meta = valid() ? find_type(data_->key_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 查询 second 的统一 value 类型。 */
  [[nodiscard]] type_id value_type() const noexcept {
    return valid() ? data_->value_type : type_id{};
  }

  /** @brief 查询 second 的统一 value 类型名。 */
  [[nodiscard]] string_name value_type_name() const noexcept {
    const std::optional<meta_type> meta = valid() ? find_type(data_->value_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 查询 second 类型，等价于 `value_type()`。 */
  [[nodiscard]] type_id second_type() const noexcept { return value_type(); }

  /** @brief 查询 second 类型名，等价于 `value_type_name()`。 */
  [[nodiscard]] string_name second_type_name() const noexcept { return value_type_name(); }

  /** @brief 获取 first 引用。 */
  [[nodiscard]] any_ref first_ref() const {
    if (!valid() || data_->first == nullptr) {
      return {};
    }

    const void* value = nullptr;
    bool value_writable = false;
    if (writable() && data_->mutable_first != nullptr) {
      value = data_->mutable_first(const_cast<void*>(storage_));
      value_writable = value != nullptr;
    } else {
      value = data_->first(storage_);
    }

    if (value == nullptr) {
      return {};
    }

    return any_ref::from_raw(data_->key_type, value, value_writable);
  }

  /** @brief 获取 second 引用。 */
  [[nodiscard]] any_ref second_ref() const {
    if (!valid() || data_->second == nullptr) {
      return {};
    }

    const void* value = nullptr;
    bool value_writable = false;
    if (writable() && data_->mutable_second != nullptr) {
      value = data_->mutable_second(const_cast<void*>(storage_));
      value_writable = value != nullptr;
    } else {
      value = data_->second(storage_);
    }

    if (value == nullptr) {
      return {};
    }

    return any_ref::from_raw(data_->value_type, value, value_writable);
  }

  /** @brief 获取 second 引用，等价于 `second_ref()`。 */
  [[nodiscard]] any_ref value_ref() const { return second_ref(); }

  /** @brief 写入 first。 */
  bool set_first(const any& value) const {
    if (!writable() || !value.valid() || value.type() != data_->key_type) {
      return false;
    }

    any_ref target = first_ref();
    return target.valid() && target.try_set(value);
  }

  /** @brief 写入 second。 */
  bool set_second(const any& value) const {
    if (!writable() || !value.valid() || value.type() != data_->value_type) {
      return false;
    }

    any_ref target = second_ref();
    return target.valid() && target.try_set(value);
  }

private:
  friend class any;
  friend class any_ref;

  pair_view(const meta_pair_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const meta_pair_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

/**
 * @brief tuple-like 运行期视图。
 *
 * `tuple_view` 不拥有 tuple，只引用 `any` 或 `any_ref` 指向的 tuple-like
 * 对象。元素类型可以逐个查询， 元素值通过下标暴露为 `any_ref`。
 */
class tuple_view final {
public:
  tuple_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询 tuple 元素数量。 */
  [[nodiscard]] std::size_t size() const noexcept { return valid() ? data_->elements.size() : 0; }

  /** @brief 查询指定元素类型。 */
  [[nodiscard]] std::optional<type_id> element_type(std::size_t index) const noexcept {
    if (!valid() || index >= data_->elements.size()) {
      return std::nullopt;
    }

    return data_->elements[index].value_type;
  }

  /** @brief 查询指定元素类型名。 */
  [[nodiscard]] string_name element_type_name(std::size_t index) const noexcept {
    const std::optional<type_id> id = element_type(index);
    const std::optional<meta_type> meta = id.has_value() ? find_type(*id) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 获取指定元素引用。 */
  [[nodiscard]] any_ref element_ref(std::size_t index) const {
    if (!valid() || index >= data_->elements.size()) {
      return {};
    }

    const meta_tuple_element_data& element_data = data_->elements[index];
    const void* value = nullptr;
    bool value_writable = false;
    if (writable() && element_data.mutable_element != nullptr) {
      value = element_data.mutable_element(const_cast<void*>(storage_));
      value_writable = value != nullptr;
    } else if (element_data.element != nullptr) {
      value = element_data.element(storage_);
    }

    if (value == nullptr) {
      return {};
    }

    return any_ref::from_raw(element_data.value_type, value, value_writable);
  }

  /** @brief 写入指定元素。 */
  bool set_element(std::size_t index, const any& value) const {
    const std::optional<type_id> expected = element_type(index);
    if (!writable() || !expected.has_value() || !value.valid() || value.type() != *expected) {
      return false;
    }

    any_ref target = element_ref(index);
    return target.valid() && target.try_set(value);
  }

private:
  friend class any;
  friend class any_ref;

  tuple_view(const meta_tuple_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const meta_tuple_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

/**
 * @brief 顺序容器运行期视图。
 *
 * `sequence_view` 不拥有容器，只引用 `any` 或 `any_ref`
 * 指向的顺序容器对象。只读视图允许查询和访问元素； 可写视图才允许清空、调整大小和追加元素。
 */
class sequence_view final {
public:
  class iterator final {
  public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = any_ref;
    using reference = any_ref;
    using pointer = void;

    iterator() noexcept = default;

    [[nodiscard]] any_ref operator*() const {
      return view_ != nullptr ? view_->element_ref(index_) : any_ref{};
    }

    iterator& operator++() noexcept {
      ++index_;
      return *this;
    }

    iterator operator++(int) noexcept {
      iterator previous = *this;
      ++(*this);
      return previous;
    }

    friend bool operator==(iterator lhs, iterator rhs) noexcept {
      return lhs.view_ == rhs.view_ && lhs.index_ == rhs.index_;
    }

    friend bool operator!=(iterator lhs, iterator rhs) noexcept { return !(lhs == rhs); }

  private:
    friend class sequence_view;

    iterator(const sequence_view* view, std::size_t index) noexcept : view_(view), index_(index) {}

    const sequence_view* view_ = nullptr;
    std::size_t index_ = 0;
  };

  sequence_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询顺序容器元素的统一 value 类型。 */
  [[nodiscard]] type_id value_type() const noexcept {
    return valid() ? data_->value_type : type_id{};
  }

  /** @brief 查询顺序容器元素的统一 value 类型名。 */
  [[nodiscard]] string_name value_type_name() const noexcept {
    const std::optional<meta_type> meta = valid() ? find_type(data_->value_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 查询顺序容器元素类型，等价于 `value_type()`。 */
  [[nodiscard]] type_id element_type() const noexcept { return value_type(); }

  /** @brief 查询顺序容器元素类型名，等价于 `value_type_name()`。 */
  [[nodiscard]] string_name element_type_name() const noexcept { return value_type_name(); }

  /** @brief 获取顺序容器元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const {
    if (!valid() || data_->size == nullptr) {
      return std::nullopt;
    }

    return data_->size(storage_);
  }

  [[nodiscard]] iterator begin() const { return valid() ? iterator{this, 0} : iterator{}; }

  [[nodiscard]] iterator end() const {
    const std::optional<std::size_t> count = size();
    return count.has_value() ? iterator{this, *count} : iterator{};
  }

  /** @brief 获取指定元素引用。 */
  [[nodiscard]] any_ref element_ref(std::size_t index) const {
    if (!valid() || data_->element == nullptr) {
      return {};
    }

    const void* element = nullptr;
    bool element_writable = false;
    if (writable() && data_->mutable_element != nullptr) {
      element = data_->mutable_element(const_cast<void*>(storage_), index);
      element_writable = element != nullptr;
    } else {
      element = data_->element(storage_, index);
    }

    if (element == nullptr) {
      return {};
    }

    return any_ref::from_raw(data_->value_type, element, element_writable);
  }

  /** @brief 清空顺序容器。 */
  bool clear() const {
    if (!writable() || data_->clear == nullptr) {
      return false;
    }

    data_->clear(const_cast<void*>(storage_));
    return true;
  }

  /** @brief 调整顺序容器大小。 */
  bool resize(std::size_t size) const {
    if (!writable() || data_->resize == nullptr) {
      return false;
    }

    return data_->resize(const_cast<void*>(storage_), size);
  }

  /** @brief 追加一个元素。 */
  bool append_value(const any& value) const {
    if (!writable() || data_->append == nullptr || !value.valid() ||
        value.type() != data_->value_type) {
      return false;
    }

    return data_->append(const_cast<void*>(storage_), value.data());
  }

private:
  friend class any;
  friend class any_ref;

  sequence_view(const meta_sequence_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const meta_sequence_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

/**
 * @brief 集合容器运行期视图。
 *
 * `set_view` 不拥有容器，只引用 `any` 或 `any_ref` 指向的集合容器对象。集合元素按 key
 * 语义暴露为只读引用； 可写视图允许清空和插入元素。
 */
class set_view final {
public:
  class iterator final {
  public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = any_ref;
    using reference = any_ref;
    using pointer = void;

    iterator() noexcept = default;

    iterator(const iterator& other) : view_(other.view_) {
      if (other.iterator_ != nullptr && view_ != nullptr && view_->data_ != nullptr &&
          view_->data_->iterator_clone != nullptr) {
        iterator_ = view_->data_->iterator_clone(other.iterator_);
      }
    }

    iterator(iterator&& other) noexcept : view_(other.view_), iterator_(other.iterator_) {
      other.view_ = nullptr;
      other.iterator_ = nullptr;
    }

    ~iterator() { reset(); }

    iterator& operator=(const iterator& other) {
      if (this == &other) {
        return *this;
      }

      iterator copy{other};
      swap(copy);
      return *this;
    }

    iterator& operator=(iterator&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      reset();
      view_ = other.view_;
      iterator_ = other.iterator_;
      other.view_ = nullptr;
      other.iterator_ = nullptr;
      return *this;
    }

    [[nodiscard]] any_ref operator*() const {
      if (view_ == nullptr || view_->data_ == nullptr || iterator_ == nullptr ||
          view_->data_->iterator_value == nullptr) {
        return {};
      }

      const void* value = view_->data_->iterator_value(iterator_);
      return any_ref::from_raw(view_->data_->value_type, value, false);
    }

    iterator& operator++() noexcept {
      if (view_ != nullptr && view_->data_ != nullptr && iterator_ != nullptr &&
          view_->data_->iterator_advance != nullptr) {
        view_->data_->iterator_advance(iterator_);
      }

      return *this;
    }

    iterator operator++(int) {
      iterator previous = *this;
      ++(*this);
      return previous;
    }

    friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept {
      if (lhs.view_ != rhs.view_) {
        return lhs.iterator_ == nullptr && rhs.iterator_ == nullptr;
      }

      if (lhs.iterator_ == nullptr || rhs.iterator_ == nullptr) {
        return true;
      }

      if (lhs.view_ == nullptr || lhs.view_->data_ == nullptr ||
          lhs.view_->data_->iterator_equal == nullptr) {
        return false;
      }

      return lhs.view_->data_->iterator_equal(lhs.iterator_, rhs.iterator_);
    }

    friend bool operator!=(const iterator& lhs, const iterator& rhs) noexcept {
      return !(lhs == rhs);
    }

  private:
    friend class set_view;

    iterator(const set_view* view, void* iterator) noexcept : view_(view), iterator_(iterator) {}

    void swap(iterator& other) noexcept {
      using std::swap;
      swap(view_, other.view_);
      swap(iterator_, other.iterator_);
    }

    void reset() noexcept {
      if (view_ != nullptr && view_->data_ != nullptr && iterator_ != nullptr &&
          view_->data_->iterator_destroy != nullptr) {
        view_->data_->iterator_destroy(iterator_);
      }

      view_ = nullptr;
      iterator_ = nullptr;
    }

    const set_view* view_ = nullptr;
    void* iterator_ = nullptr;
  };

  set_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询集合元素的统一 value 类型。 */
  [[nodiscard]] type_id value_type() const noexcept {
    return valid() ? data_->value_type : type_id{};
  }

  /** @brief 查询集合元素的统一 value 类型名。 */
  [[nodiscard]] string_name value_type_name() const noexcept {
    const std::optional<meta_type> meta = valid() ? find_type(data_->value_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 查询集合元素类型，等价于 `value_type()`。 */
  [[nodiscard]] type_id element_type() const noexcept { return value_type(); }

  /** @brief 查询集合元素类型名，等价于 `value_type_name()`。 */
  [[nodiscard]] string_name element_type_name() const noexcept { return value_type_name(); }

  /** @brief 获取集合元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const {
    if (!valid() || data_->size == nullptr) {
      return std::nullopt;
    }

    return data_->size(storage_);
  }

  [[nodiscard]] iterator begin() const {
    if (!has_iterator_support()) {
      return {};
    }

    return iterator{this, data_->begin(storage_)};
  }

  [[nodiscard]] iterator end() const {
    if (!has_iterator_support()) {
      return {};
    }

    return iterator{this, data_->end(storage_)};
  }

  /** @brief 遍历集合元素只读引用。 */
  bool for_each_ref(set_element_visitor visitor, void* user_data = nullptr) const {
    if (!valid() || data_->visit == nullptr || visitor == nullptr) {
      return false;
    }

    struct visit_context {
      const meta_set_data* data = nullptr;
      set_element_visitor visitor = nullptr;
      void* user_data = nullptr;
    };

    visit_context context{data_, visitor, user_data};
    return data_->visit(
        storage_,
        [](const void* element, void* raw_context) -> bool {
          auto* context = static_cast<visit_context*>(raw_context);
          const meta_set_data* data = context->data;
          any_ref element_ref = any_ref::from_raw(data->value_type, element, false);

          return context->visitor(element_ref, context->user_data);
        },
        &context);
  }

  /** @brief 清空集合。 */
  bool clear() const {
    if (!writable() || data_->clear == nullptr) {
      return false;
    }

    data_->clear(const_cast<void*>(storage_));
    return true;
  }

  /** @brief 插入一个元素。 */
  bool insert_value(const any& value) const {
    if (!writable() || data_->insert == nullptr || !value.valid() ||
        value.type() != data_->value_type) {
      return false;
    }

    return data_->insert(const_cast<void*>(storage_), value.data());
  }

private:
  friend class any;
  friend class any_ref;

  set_view(const meta_set_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  [[nodiscard]] bool has_iterator_support() const noexcept {
    return valid() && data_->begin != nullptr && data_->end != nullptr &&
           data_->iterator_clone != nullptr && data_->iterator_destroy != nullptr &&
           data_->iterator_advance != nullptr && data_->iterator_equal != nullptr &&
           data_->iterator_value != nullptr;
  }

  const meta_set_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

/**
 * @brief 关联容器运行期视图。
 *
 * `associative_view` 不拥有容器，只引用 `any` 或 `any_ref`
 * 指向的关联容器对象。只读视图允许查询和遍历； 可写视图才允许清空和插入。
 */
class associative_view final {
public:
  class iterator final {
  public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = associative_entry_ref;
    using reference = associative_entry_ref;
    using pointer = void;

    iterator() noexcept = default;

    iterator(const iterator& other) : view_(other.view_) {
      if (other.iterator_ != nullptr && view_ != nullptr && view_->data_ != nullptr &&
          view_->data_->iterator_clone != nullptr) {
        iterator_ = view_->data_->iterator_clone(other.iterator_);
      }
    }

    iterator(iterator&& other) noexcept : view_(other.view_), iterator_(other.iterator_) {
      other.view_ = nullptr;
      other.iterator_ = nullptr;
    }

    ~iterator() { reset(); }

    iterator& operator=(const iterator& other) {
      if (this == &other) {
        return *this;
      }

      iterator copy{other};
      swap(copy);
      return *this;
    }

    iterator& operator=(iterator&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      reset();
      view_ = other.view_;
      iterator_ = other.iterator_;
      other.view_ = nullptr;
      other.iterator_ = nullptr;
      return *this;
    }

    [[nodiscard]] associative_entry_ref operator*() const {
      if (view_ == nullptr || view_->data_ == nullptr || iterator_ == nullptr ||
          view_->data_->iterator_key == nullptr || view_->data_->iterator_value == nullptr) {
        return {};
      }

      const void* key = view_->data_->iterator_key(iterator_);
      const void* value = view_->data_->iterator_value(iterator_);
      return associative_entry_ref{
          any_ref::from_raw(view_->data_->key_type, key, false),
          any_ref::from_raw(view_->data_->value_type, value, false),
      };
    }

    iterator& operator++() noexcept {
      if (view_ != nullptr && view_->data_ != nullptr && iterator_ != nullptr &&
          view_->data_->iterator_advance != nullptr) {
        view_->data_->iterator_advance(iterator_);
      }

      return *this;
    }

    iterator operator++(int) {
      iterator previous = *this;
      ++(*this);
      return previous;
    }

    friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept {
      if (lhs.view_ != rhs.view_) {
        return lhs.iterator_ == nullptr && rhs.iterator_ == nullptr;
      }

      if (lhs.iterator_ == nullptr || rhs.iterator_ == nullptr) {
        return true;
      }

      if (lhs.view_ == nullptr || lhs.view_->data_ == nullptr ||
          lhs.view_->data_->iterator_equal == nullptr) {
        return false;
      }

      return lhs.view_->data_->iterator_equal(lhs.iterator_, rhs.iterator_);
    }

    friend bool operator!=(const iterator& lhs, const iterator& rhs) noexcept {
      return !(lhs == rhs);
    }

  private:
    friend class associative_view;

    iterator(const associative_view* view, void* iterator) noexcept
        : view_(view), iterator_(iterator) {}

    void swap(iterator& other) noexcept {
      using std::swap;
      swap(view_, other.view_);
      swap(iterator_, other.iterator_);
    }

    void reset() noexcept {
      if (view_ != nullptr && view_->data_ != nullptr && iterator_ != nullptr &&
          view_->data_->iterator_destroy != nullptr) {
        view_->data_->iterator_destroy(iterator_);
      }

      view_ = nullptr;
      iterator_ = nullptr;
    }

    const associative_view* view_ = nullptr;
    void* iterator_ = nullptr;
  };

  associative_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  [[nodiscard]] type_id key_type() const noexcept { return valid() ? data_->key_type : type_id{}; }

  [[nodiscard]] string_name key_type_name() const noexcept {
    const std::optional<meta_type> meta = valid() ? find_type(data_->key_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 查询关联容器 value 类型。 */
  [[nodiscard]] type_id value_type() const noexcept {
    return valid() ? data_->value_type : type_id{};
  }

  /** @brief 查询关联容器 value 类型名。 */
  [[nodiscard]] string_name value_type_name() const noexcept {
    const std::optional<meta_type> meta = valid() ? find_type(data_->value_type) : std::nullopt;
    return meta ? meta->name() : string_name{};
  }

  /** @brief 查询关联容器 mapped 类型，等价于 `value_type()`。 */
  [[nodiscard]] type_id mapped_type() const noexcept { return value_type(); }

  /** @brief 查询关联容器 mapped 类型名，等价于 `value_type_name()`。 */
  [[nodiscard]] string_name mapped_type_name() const noexcept { return value_type_name(); }

  /** @brief 获取关联容器元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const {
    if (!valid() || data_->size == nullptr) {
      return std::nullopt;
    }

    return data_->size(storage_);
  }

  [[nodiscard]] iterator begin() const {
    if (!has_iterator_support()) {
      return {};
    }

    return iterator{this, data_->begin(storage_)};
  }

  [[nodiscard]] iterator end() const {
    if (!has_iterator_support()) {
      return {};
    }

    return iterator{this, data_->end(storage_)};
  }

  /** @brief 遍历关联容器 key/value 只读引用。 */
  bool for_each_ref(associative_entry_visitor visitor, void* user_data = nullptr) const {
    if (!valid() || data_->visit == nullptr || visitor == nullptr) {
      return false;
    }

    struct visit_context {
      const meta_associative_data* data = nullptr;
      associative_entry_visitor visitor = nullptr;
      void* user_data = nullptr;
    };

    visit_context context{data_, visitor, user_data};
    return data_->visit(
        storage_,
        [](const void* key, const void* mapped, void* raw_context) -> bool {
          auto* context = static_cast<visit_context*>(raw_context);
          const meta_associative_data* data = context->data;
          any_ref key_ref = any_ref::from_raw(data->key_type, key, false);
          any_ref mapped_ref = any_ref::from_raw(data->value_type, mapped, false);

          return context->visitor(key_ref, mapped_ref, context->user_data);
        },
        &context);
  }

  /** @brief 清空关联容器。 */
  bool clear() const {
    if (!writable() || data_->clear == nullptr) {
      return false;
    }

    data_->clear(const_cast<void*>(storage_));
    return true;
  }

  /** @brief 插入一组 key/value。 */
  bool insert_value(const any& key, const any& mapped) const {
    if (!writable() || data_->insert == nullptr || !key.valid() || !mapped.valid() ||
        key.type() != data_->key_type || mapped.type() != data_->value_type) {
      return false;
    }

    return data_->insert(const_cast<void*>(storage_), key.data(), mapped.data());
  }

private:
  friend class any;
  friend class any_ref;

  associative_view(const meta_associative_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  [[nodiscard]] bool has_iterator_support() const noexcept {
    return valid() && data_->begin != nullptr && data_->end != nullptr &&
           data_->iterator_clone != nullptr && data_->iterator_destroy != nullptr &&
           data_->iterator_advance != nullptr && data_->iterator_equal != nullptr &&
           data_->iterator_key != nullptr && data_->iterator_value != nullptr;
  }

  const meta_associative_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

inline associative_view any::create_associative_view() {
  if (!valid()) {
    return {};
  }

  const meta_associative_data* data = get_meta_registry().find_ptr<meta_associative_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return associative_view{data, mutable_data(), true};
}

inline associative_view any::create_associative_view() const {
  if (!valid()) {
    return {};
  }

  const meta_associative_data* data = get_meta_registry().find_ptr<meta_associative_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return associative_view{data, data_, false};
}

inline associative_view any_ref::create_associative_view() const {
  if (!valid()) {
    return {};
  }

  const meta_associative_data* data = get_meta_registry().find_ptr<meta_associative_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return associative_view{data, data_, writable_};
}

inline pair_view any::create_pair_view() {
  if (!valid()) {
    return {};
  }

  const meta_pair_data* data = get_meta_registry().find_ptr<meta_pair_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return pair_view{data, mutable_data(), true};
}

inline pair_view any::create_pair_view() const {
  if (!valid()) {
    return {};
  }

  const meta_pair_data* data = get_meta_registry().find_ptr<meta_pair_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return pair_view{data, data_, false};
}

inline pair_view any_ref::create_pair_view() const {
  if (!valid()) {
    return {};
  }

  const meta_pair_data* data = get_meta_registry().find_ptr<meta_pair_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return pair_view{data, data_, writable_};
}

inline tuple_view any::create_tuple_view() {
  if (!valid()) {
    return {};
  }

  const meta_tuple_data* data = get_meta_registry().find_ptr<meta_tuple_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return tuple_view{data, mutable_data(), true};
}

inline tuple_view any::create_tuple_view() const {
  if (!valid()) {
    return {};
  }

  const meta_tuple_data* data = get_meta_registry().find_ptr<meta_tuple_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return tuple_view{data, data_, false};
}

inline tuple_view any_ref::create_tuple_view() const {
  if (!valid()) {
    return {};
  }

  const meta_tuple_data* data = get_meta_registry().find_ptr<meta_tuple_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return tuple_view{data, data_, writable_};
}

inline indirect_view any::create_indirect_view() {
  if (!valid()) {
    return {};
  }

  const meta_type_semantics_data* data =
      get_meta_registry().find_ptr<meta_type_semantics_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return indirect_view{data, mutable_data(), true};
}

inline indirect_view any::create_indirect_view() const {
  if (!valid()) {
    return {};
  }

  const meta_type_semantics_data* data =
      get_meta_registry().find_ptr<meta_type_semantics_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return indirect_view{data, data_, false};
}

inline indirect_view any_ref::create_indirect_view() const {
  if (!valid()) {
    return {};
  }

  const meta_type_semantics_data* data =
      get_meta_registry().find_ptr<meta_type_semantics_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return indirect_view{data, data_, writable_};
}

inline sequence_view any::create_sequence_view() {
  if (!valid()) {
    return {};
  }

  const meta_sequence_data* data = get_meta_registry().find_ptr<meta_sequence_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return sequence_view{data, mutable_data(), true};
}

inline sequence_view any::create_sequence_view() const {
  if (!valid()) {
    return {};
  }

  const meta_sequence_data* data = get_meta_registry().find_ptr<meta_sequence_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return sequence_view{data, data_, false};
}

inline sequence_view any_ref::create_sequence_view() const {
  if (!valid()) {
    return {};
  }

  const meta_sequence_data* data = get_meta_registry().find_ptr<meta_sequence_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return sequence_view{data, data_, writable_};
}

inline set_view any::create_set_view() {
  if (!valid()) {
    return {};
  }

  const meta_set_data* data = get_meta_registry().find_ptr<meta_set_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return set_view{data, mutable_data(), true};
}

inline set_view any::create_set_view() const {
  if (!valid()) {
    return {};
  }

  const meta_set_data* data = get_meta_registry().find_ptr<meta_set_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return set_view{data, data_, false};
}

inline set_view any_ref::create_set_view() const {
  if (!valid()) {
    return {};
  }

  const meta_set_data* data = get_meta_registry().find_ptr<meta_set_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return set_view{data, data_, writable_};
}

} // namespace caors

#endif
