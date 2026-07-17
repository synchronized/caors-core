// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_ANY_H_
#define CAORS_META_VALUE_ANY_H_

#include "caors/memory/resource.h"
#include "caors/meta/capability_facade.h"
#include "caors/meta/query.h"
#include "caors/meta/value/fwd.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta {

[[nodiscard]] owned_value make_owned_value(meta::type type, void* value,
                                           memory::memory_resource* resource,
                                           meta::resource_destroy_fn destroy) noexcept;
[[nodiscard]] owned_value make_owned_value(meta::type type, void* value,
                                           meta::destroy_fn destroy) noexcept;

/** @brief `owned_value` 释放所有权后的裸对象和匹配销毁函数。 */
struct released_owned_value {
  meta::type type{};
  void* value = nullptr;
  memory::memory_resource* resource = nullptr;
  meta::resource_destroy_fn resource_destroy = nullptr;
  meta::destroy_fn destroy = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return type && value != nullptr && (resource_destroy != nullptr || destroy != nullptr);
  }
};

/** @brief 对已 release 的 owned value 重新提供失败路径 RAII 销毁保护。 */
class released_owned_value_guard final {
public:
  released_owned_value_guard() noexcept = default;
  explicit released_owned_value_guard(released_owned_value value) noexcept : value_(value) {}
  ~released_owned_value_guard() noexcept { reset(); }

  released_owned_value_guard(const released_owned_value_guard&) = delete;
  released_owned_value_guard& operator=(const released_owned_value_guard&) = delete;

  released_owned_value_guard(released_owned_value_guard&& other) noexcept
      : value_(std::exchange(other.value_, {})) {}

  released_owned_value_guard& operator=(released_owned_value_guard&& other) noexcept {
    if (this != &other) {
      reset();
      value_ = std::exchange(other.value_, {});
    }
    return *this;
  }

  [[nodiscard]] bool valid() const noexcept { return value_.valid(); }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept { return value_.type; }
  [[nodiscard]] void* data() const noexcept { return value_.value; }
  [[nodiscard]] memory::memory_resource* resource() const noexcept { return value_.resource; }
  [[nodiscard]] meta::resource_destroy_fn resource_destroy() const noexcept {
    return value_.resource_destroy;
  }
  [[nodiscard]] meta::destroy_fn destroy() const noexcept { return value_.destroy; }

  /** @brief 释放 guard 的销毁责任并返回原始 release 结果。 */
  [[nodiscard]] released_owned_value release() noexcept { return std::exchange(value_, {}); }

  /** @brief 外部已经接管销毁责任时，清空 guard 且不销毁对象。 */
  void dismiss() noexcept { value_ = {}; }

  /** @brief 销毁当前托管对象并清空 guard。 */
  void reset() noexcept {
    if (!value_.valid()) {
      return;
    }

    if (value_.resource_destroy != nullptr) {
      value_.resource_destroy(value_.resource, value_.value);
    } else if (value_.destroy != nullptr) {
      value_.destroy(value_.value);
    }

    value_ = {};
  }

private:
  released_owned_value value_{};
};

/**
 * @brief 拥有一个运行期构造对象的轻量 RAII 句柄。
 *
 * `owned_value` 主要用于 factory、constructor 和反序列化内部创建对象后再决定如何转移所有权。
 * 普通用户通常直接使用 `construct()` 得到 `any`；需要先填充对象、再交给 wrapper 或容器接管时，
 * 可以使用 `construct_owned()`。
 */
class owned_value final {
public:
  owned_value() noexcept = default;
  ~owned_value();

  owned_value(const owned_value&) = delete;
  owned_value& operator=(const owned_value&) = delete;

  owned_value(owned_value&& other) noexcept;
  owned_value& operator=(owned_value&& other) noexcept;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept;
  [[nodiscard]] const void* data() const noexcept;
  [[nodiscard]] void* mutable_data() noexcept;
  [[nodiscard]] any_ref ref();
  [[nodiscard]] any_ref ref() const;

  /**
   * @brief 将当前所有权转移为 `any`。
   *
   * 转移成功后当前对象变为空；失败时仍由当前对象负责销毁原始值。
   */
  [[nodiscard]] any into_any() &&;

  /**
   * @brief 释放当前对象所有权并让当前句柄变为空。
   *
   * 这是高级所有权 API。调用方必须保存返回值里的类型、资源和销毁函数，并负责最终销毁
   * `value`，否则会泄漏。
   */
  [[nodiscard]] released_owned_value release() && noexcept;

  /** @brief 销毁当前持有的对象并清空句柄。 */
  void reset() noexcept;

  /** @brief 交换两个 owned value。 */
  void swap(owned_value& other) noexcept;

private:
  friend class any;
  friend owned_value make_owned_value(meta::type, void*, memory::memory_resource*,
                                      meta::resource_destroy_fn) noexcept;
  friend owned_value make_owned_value(meta::type, void*, meta::destroy_fn) noexcept;

  void clear_released() noexcept;

  meta::type type_{};
  void* data_ = nullptr;
  memory::memory_resource* resource_ = nullptr;
  meta::resource_destroy_fn resource_destroy_ = nullptr;
  meta::destroy_fn default_destroy_ = nullptr;
};

void swap(owned_value& lhs, owned_value& rhs) noexcept;

[[nodiscard]] inline owned_value make_owned_value(meta::type type, void* value,
                                                  memory::memory_resource* resource,
                                                  meta::resource_destroy_fn destroy) noexcept {
  if (!type || value == nullptr || destroy == nullptr) {
    return {};
  }

  owned_value result;
  result.type_ = type;
  result.data_ = value;
  result.resource_ = resource;
  result.resource_destroy_ = destroy;
  return result;
}

[[nodiscard]] inline owned_value make_owned_value(meta::type type, void* value,
                                                  meta::destroy_fn destroy) noexcept {
  if (!type || value == nullptr || destroy == nullptr) {
    return {};
  }

  owned_value result;
  result.type_ = type;
  result.data_ = value;
  result.default_destroy_ = destroy;
  return result;
}

/**
 * @brief 运行期 C++ 类型值。
 *
 * `any` 持有一份对象拷贝，并保存对应的运行期类型信息。它不引用原对象，因此可以安全地
 * 脱离字段所属对象生命周期使用。第一版只支持可拷贝类型；不可拷贝对象会产生无效 `any`。
 */
class any final {
public:
  any() noexcept = default;
  ~any();

  any(const any& other);
  any(any&& other) noexcept;
  any& operator=(const any& other);
  any& operator=(any&& other) noexcept;

  /**
   * @brief 从 C++ 值创建运行期值。
   *
   * 无显式类型名时，值会使用 C++ 类型 descriptor 创建；该类型不要求已经注册到
   * 全局 meta 注册表。
   */
  template <typename T> [[nodiscard]] static any from(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(std::is_copy_constructible_v<type>, "any requires copy constructible types");
    return copy_from_raw(meta::resolve<type>(), &source);
  }

  /** @brief 从 C++ 值和显式类型名创建运行期值；类型名必须已经注册并匹配该 C++ 类型。 */
  template <typename T> [[nodiscard]] static any from(const T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(std::is_copy_constructible_v<type>, "any requires copy constructible types");

    const meta::type registered = caors::meta::find<type>();
    if (!registered.registered() || registered.name().empty() || registered.name() != name) {
      return {};
    }

    return copy_from_raw(registered, &source);
  }

  /** @brief 按运行期类型 id 默认构造一个值。不可默认构造类型会返回无效 `any`。 */
  [[nodiscard]] static any default_construct(type_id type);

  /** @brief 按运行期类型视图默认构造一个值。不可默认构造类型会返回无效 `any`。 */
  [[nodiscard]] static any default_construct(meta::type type);

  /**
   * @brief 按运行期类型 id 用指定内存资源默认构造一个值。
   *
   * 返回的 `any`
   * 会记住对应资源，并在析构时用匹配的资源销毁函数释放对象。若类型没有资源感知构造函数表，
   * 会回退到普通默认构造。
   */
  [[nodiscard]] static any default_construct(type_id type, memory::memory_resource* resource);

  /**
   * @brief 按运行期类型视图用指定内存资源默认构造一个值。
   *
   * `type` 可以是已注册类型，也可以是 `meta::resolve<T>()` 返回的 descriptor 视图。
   */
  [[nodiscard]] static any default_construct(meta::type type, memory::memory_resource* resource);

  /**
   * @brief 从原始地址复制构造运行期值。
   *
   * 调用方必须保证 `source` 指向与 `type` 匹配的有效对象；类型不可复制或地址为空时返回无效
   * `any`。
   */
  [[nodiscard]] static any copy_from_raw(meta::type type, const void* source);

  /** @brief 接管一个 `owned_value` 持有的对象。转移成功后 `owned_value` 变为空。 */
  [[nodiscard]] static any take_ownership(owned_value&& value);

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept;
  [[nodiscard]] const void* data() const noexcept;
  [[nodiscard]] void* mutable_data() noexcept;
  [[nodiscard]] any_ref ref();
  [[nodiscard]] any_ref ref() const;
  template <typename Capability> [[nodiscard]] auto get_capability();
  template <typename Capability> [[nodiscard]] auto get_capability() const;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;

  /** @brief 清空当前值。 */
  void reset() noexcept;

  /** @brief 交换两个运行期值。 */
  void swap(any& other) noexcept;

  /** @brief 判断当前值是否匹配指定 C++ 类型。 */
  template <typename T> [[nodiscard]] bool matches_type() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const meta::type expected = caors::meta::resolve<type>();
    return valid() && expected && get_type().id() == expected.id() && size() == sizeof(type) &&
           alignment() == alignof(type);
  }

  /** @brief 判断两个运行期值是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any& other) const noexcept;

  /** @brief 按指定类型获取只读指针。 */
  template <typename T>
  [[nodiscard]] const std::remove_cv_t<std::remove_reference_t<T>>* try_get() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>()) {
      return nullptr;
    }

    return static_cast<const type*>(data());
  }

  /** @brief 按指定类型获取可写指针。 */
  template <typename T>
  [[nodiscard]] std::remove_cv_t<std::remove_reference_t<T>>* try_get_mutable() noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>()) {
      return nullptr;
    }

    return static_cast<type*>(mutable_data());
  }

  /** @brief 从同类型 C++ 值写入当前值。 */
  template <typename T> bool try_set(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const meta::value_storage_ops* storage = type_.get_raw_ops<meta::value_storage_ops>();
    if (!matches_type<type>() || storage == nullptr || storage->ops.assign == nullptr) {
      return false;
    }

    return storage->ops.assign(data_, &source);
  }

  /** @brief 从同类型运行期值写入当前值。 */
  bool try_set(const any& source);

private:
  friend class any_ref;

  enum class storage_mode : unsigned char {
    empty,
    inline_value,
    heap_value,
    resource_value,
  };

  static constexpr std::size_t inline_storage_size = sizeof(void*) * 3;
  static constexpr std::size_t inline_storage_alignment = alignof(std::max_align_t);

  void copy_from(const any& other);
  void move_from(any& other) noexcept;
  [[nodiscard]] void* inline_data() noexcept;
  [[nodiscard]] const void* inline_data() const noexcept;
  [[nodiscard]] bool stores_inline() const noexcept { return mode_ == storage_mode::inline_value; }

  meta::type type_{};
  void* data_ = nullptr;
  memory::memory_resource* resource_ = nullptr;
  meta::resource_destroy_fn resource_destroy_ = nullptr;
  meta::destroy_fn destroy_ = nullptr;
  storage_mode mode_ = storage_mode::empty;
  alignas(std::max_align_t) unsigned char inline_storage_[inline_storage_size]{};
};

void swap(any& lhs, any& rhs) noexcept;

inline owned_value factory_capability::construct_owned(memory::memory_resource* resource) const {
  void* value = valid() ? ops_->construct(resource) : nullptr;
  return make_owned_value(get_type(), value, resource, valid() ? ops_->destroy : nullptr);
}

inline any factory_capability::construct(memory::memory_resource* resource) const {
  return std::move(construct_owned(resource)).into_any();
}

inline owned_value
constructor_capability::construct_owned(std::span<const any_ref> arguments,
                                        memory::memory_resource* resource) const {
  void* value = matches(arguments.size()) ? ops_->construct(resource, arguments) : nullptr;
  return make_owned_value(get_type(), value, resource, valid() ? ops_->destroy : nullptr);
}

inline any constructor_capability::construct(std::span<const any_ref> arguments,
                                             memory::memory_resource* resource) const {
  return std::move(construct_owned(arguments, resource)).into_any();
}

} // namespace caors::meta

namespace caors {

using meta::any;
using meta::owned_value;
using meta::released_owned_value;
using meta::released_owned_value_guard;

} // namespace caors

#endif
