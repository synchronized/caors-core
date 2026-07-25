// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_ANY_REF_H_
#define CAORS_META_VALUE_ANY_REF_H_

#include "caors/meta/value/any.h"

namespace caors::meta {

namespace detail {
struct any_ref_access;
}

/**
 * @brief 运行期 C++ 类型引用。
 *
 * `any_ref` 不拥有对象，只保存外部对象地址和运行期类型信息。调用方必须保证被引用对象在
 * `any_ref` 使用期间仍然存活；可写引用只能从非 const 对象或可写属性创建。
 */
class CAORS_API any_ref final {
public:
  any_ref() noexcept = default;

  /** @brief 从可写 C++ 对象创建引用。 */
  template <typename T> [[nodiscard]] static any_ref from(T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    return from(meta::resolve<type>(), &source);
  }

  /** @brief 从可写 C++ 对象和显式类型名创建引用；类型名必须已经注册并匹配该 C++ 类型。 */
  template <typename T> [[nodiscard]] static any_ref from(T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const meta::type registered = caors::meta::find<type>();
    if (!registered.registered() || registered.name().empty() || registered.name() != name) {
      return {};
    }

    return from(registered, &source);
  }

  /** @brief 从只读 C++ 对象创建引用。 */
  template <typename T> [[nodiscard]] static any_ref from_const(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    return from(meta::resolve<type>(), &source);
  }

  /** @brief 从只读 C++ 对象和显式类型名创建引用；类型名必须已经注册并匹配该 C++ 类型。 */
  template <typename T>
  [[nodiscard]] static any_ref from_const(const T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const meta::type registered = caors::meta::find<type>();
    if (!registered.registered() || registered.name().empty() || registered.name() != name) {
      return {};
    }

    return from(registered, &source);
  }

  /**
   * @brief 从显式运行期类型和已知 C++ 类型地址创建引用。
   *
   * `runtime_type` 必须与 `T` 的类型 ID、大小和对齐一致。引用的可写性由 `T` 的 const
   * 修饰自动推导；空地址、volatile 类型或类型不匹配时返回无效引用。
   */
  template <typename T>
  [[nodiscard]] static any_ref from(meta::type runtime_type, T* source) noexcept {
    using value_type = std::remove_cv_t<T>;
    static_assert(!std::is_void_v<value_type>, "any_ref cannot reference void");

    if constexpr (std::is_volatile_v<T>) {
      return {};
    } else {
      const meta::type expected = meta::resolve<value_type>();
      if (source == nullptr || !runtime_type || !expected || runtime_type.id() != expected.id() ||
          runtime_type.size() != sizeof(value_type) ||
          runtime_type.alignment() != alignof(value_type)) {
        return {};
      }

      return from_erased(runtime_type, source, !std::is_const_v<T>);
    }
  }

  [[nodiscard]] bool valid() const noexcept;

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta::type get_type() const noexcept;
  [[nodiscard]] const void* data() const noexcept;
  [[nodiscard]] void* mutable_data() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;
  [[nodiscard]] bool writable() const noexcept;
  [[nodiscard]] bool assignable() const noexcept;

  /**
   * @brief 查询并绑定运行期实例能力。
   *
   * 推荐传入类型级 capability facade，例如 `sequence_capability`；返回类型由
   * `capability_view_traits` 决定，通常是绑定到当前对象的实例 view。
   */
  template <typename Capability>
  [[nodiscard]] auto get_capability() const -> typename meta::capability_view_traits<
      std::remove_cv_t<std::remove_reference_t<Capability>>>::result_type {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    using traits_type = meta::capability_view_traits<capability_type>;
    static_assert(traits_type::supported, "Unsupported CAORS capability view");
    return traits_type::bind(*this);
  }

  /** @brief 判断当前引用是否匹配指定 C++ 类型。 */
  template <typename T> [[nodiscard]] bool matches_type() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const meta::type expected = caors::meta::resolve<type>();
    return valid() && expected && get_type().id() == expected.id() && size() == sizeof(type) &&
           alignment() == alignof(type);
  }

  /** @brief 判断两个运行期值是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any& other) const noexcept;

  /** @brief 判断两个运行期引用是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any_ref& other) const noexcept;

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
    const meta::value_storage_ops* storage = type_.get_raw_ops<meta::value_storage_ops>();
    if (!matches_type<type>() || !writable_ || storage == nullptr ||
        storage->ops.assign == nullptr) {
      return false;
    }

    return storage->ops.assign(const_cast<void*>(data_), &source);
  }

  /** @brief 从同类型运行期值写入当前引用指向的对象。 */
  bool try_set(const any& source) const;

  /** @brief 从同类型运行期引用写入当前引用指向的对象。 */
  bool try_set(const any_ref& source) const;

  /** @brief 尝试复制引用指向的对象。不可拷贝对象会返回无效 `any`。 */
  [[nodiscard]] any to_any() const;

private:
  friend struct detail::any_ref_access;
  friend class any;
  friend class indirect_view;
  friend class sequence_view;
  friend class set_view;
  friend class associative_view;
  friend class pair_view;
  friend class tuple_view;
  friend class variant_view;

  [[nodiscard]] static any_ref from_erased(meta::type type, const void* data,
                                           bool writable) noexcept;

  meta::type type_{};
  const void* data_ = nullptr;
  bool writable_ = false;
};

namespace detail {

/** @brief 为内部完全擦除流程提供 any_ref 构造入口。 */
struct any_ref_access final {
  [[nodiscard]] static any_ref from_erased(meta::type type, const void* data,
                                           bool writable) noexcept {
    return any_ref::from_erased(type, data, writable);
  }
};

} // namespace detail

} // namespace caors::meta

namespace caors::meta {

template <typename Capability> [[nodiscard]] auto any::get_capability() {
  return ref().template get_capability<Capability>();
}

template <typename Capability> [[nodiscard]] auto any::get_capability() const {
  return ref().template get_capability<Capability>();
}

} // namespace caors::meta

namespace caors {

using meta::any_ref;

} // namespace caors

#endif
