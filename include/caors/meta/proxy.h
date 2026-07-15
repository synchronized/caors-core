// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_PROXY_H_
#define CAORS_META_PROXY_H_

#include "caors/meta/capability.h"

#include <type_traits>
#include <utility>

namespace caors::meta {

/**
 * @brief C++ 类型到 capability ops 的声明式 model 扩展点。
 *
 * 默认没有 model。用户可以特化该模板，或继承 `meta::proxy::model`，让
 * `meta::class_<T>().capability<Capability>()` 自动注册对应 `ops`。
 */
template <typename Capability, typename T> struct capability_model {};

namespace proxy {

namespace detail {

template <typename MemberPointer> struct member_object_pointer_traits;

template <typename Class, typename Member> struct member_object_pointer_traits<Member Class::*> {
  using class_type = Class;
  using value_type = Member;
};

template <auto Member, typename T, typename FunctionPointer> struct method_callback;

template <auto Member, typename T, typename R, typename... Args>
struct method_callback<Member, T, R (*)(const void*, Args...)> {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  static_assert(std::is_member_function_pointer_v<decltype(Member)>,
                "proxy::method requires a member function pointer");

  [[nodiscard]] static R call(const void* object, Args... args) {
    if constexpr (std::is_void_v<R>) {
      if (object != nullptr) {
        (static_cast<const type*>(object)->*Member)(std::forward<Args>(args)...);
      }
    } else {
      if (object == nullptr) {
        return R{};
      }

      return (static_cast<const type*>(object)->*Member)(std::forward<Args>(args)...);
    }
  }
};

template <auto Member, typename T, typename R, typename... Args>
struct method_callback<Member, T, R (*)(void*, Args...)> {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  static_assert(std::is_member_function_pointer_v<decltype(Member)>,
                "proxy::method requires a member function pointer");

  static R call(void* object, Args... args) {
    if constexpr (std::is_void_v<R>) {
      if (object != nullptr) {
        (static_cast<type*>(object)->*Member)(std::forward<Args>(args)...);
      }
    } else {
      if (object == nullptr) {
        return R{};
      }

      return (static_cast<type*>(object)->*Member)(std::forward<Args>(args)...);
    }
  }
};

template <auto Member, typename T, typename R, typename... Args>
struct method_callback<Member, T, R (*)(const void*, Args...) noexcept> {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  static_assert(std::is_member_function_pointer_v<decltype(Member)>,
                "proxy::method requires a member function pointer");

  [[nodiscard]] static R call(const void* object, Args... args) noexcept {
    if constexpr (std::is_void_v<R>) {
      if (object != nullptr) {
        (static_cast<const type*>(object)->*Member)(std::forward<Args>(args)...);
      }
    } else {
      if (object == nullptr) {
        return R{};
      }

      return (static_cast<const type*>(object)->*Member)(std::forward<Args>(args)...);
    }
  }
};

template <auto Member, typename T, typename R, typename... Args>
struct method_callback<Member, T, R (*)(void*, Args...) noexcept> {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  static_assert(std::is_member_function_pointer_v<decltype(Member)>,
                "proxy::method requires a member function pointer");

  static R call(void* object, Args... args) noexcept {
    if constexpr (std::is_void_v<R>) {
      if (object != nullptr) {
        (static_cast<type*>(object)->*Member)(std::forward<Args>(args)...);
      }
    } else {
      if (object == nullptr) {
        return R{};
      }

      return (static_cast<type*>(object)->*Member)(std::forward<Args>(args)...);
    }
  }
};

} // namespace detail

/**
 * @brief 把一个 `ops` 字段绑定到 C++ 类型的成员函数。
 *
 * `OpsMember` 是 `ops` 内部函数指针字段，例如 `&matrix_ops::rows`；
 * `Member` 是具体 C++ 类型成员函数，例如 `&matrix::rows`。
 */
template <auto OpsMember, auto Member> struct method {
  template <typename T, typename Ops> static void apply(Ops& ops) {
    using ops_type = std::remove_cv_t<std::remove_reference_t<Ops>>;
    using ops_member_traits = detail::member_object_pointer_traits<decltype(OpsMember)>;
    using field_type = typename ops_member_traits::value_type;

    static_assert(std::is_same_v<typename ops_member_traits::class_type, ops_type>,
                  "proxy::method ops member does not belong to the target ops type");

    ops.*OpsMember = &detail::method_callback<Member, T, field_type>::call;
  }
};

/**
 * @brief 用一组声明式绑定为 `Capability + T` 生成静态 ops。
 */
template <typename Capability, typename T, typename... Bindings> struct model {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using ops_type = typename capability_type::ops_type;

  inline static const ops_type value = [] {
    ops_type ops{};
    (Bindings::template apply<value_type>(ops), ...);
    return ops;
  }();

  [[nodiscard]] static const ops_type* get() noexcept {
    return meta::detail::capability_ops_valid(&value) ? &value : nullptr;
  }
};

} // namespace proxy

namespace detail {

template <typename Capability, typename T, typename = void>
struct has_capability_model : std::false_type {};

template <typename Capability, typename T>
struct has_capability_model<
    Capability, T,
    std::void_t<typename capability_model<std::remove_cv_t<std::remove_reference_t<Capability>>,
                                          std::remove_cv_t<std::remove_reference_t<T>>>::ops_type,
                decltype(capability_model<std::remove_cv_t<std::remove_reference_t<Capability>>,
                                          std::remove_cv_t<std::remove_reference_t<T>>>::get())>> {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using model_type = capability_model<capability_type, value_type>;
  using ops_type = typename model_type::ops_type;

  static constexpr bool value = std::is_same_v<ops_type, typename capability_type::ops_type> &&
                                std::is_convertible_v<decltype(model_type::get()), const ops_type*>;
};

template <typename Capability, typename T>
inline constexpr bool has_capability_model_v = has_capability_model<Capability, T>::value;

/** @brief 把 `capability_model<Capability, T>` 生成的 ops 挂到运行期索引。内部入口。 */
template <typename Capability, typename T> [[nodiscard]] error register_capability_model_for() {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using model_type = capability_model<capability_type, value_type>;
  using ops_type = typename model_type::ops_type;

  static_assert(std::is_same_v<ops_type, typename capability_type::ops_type>,
                "capability_model ops_type must match Capability::ops_type");

  const ops_type* ops = model_type::get();
  if (ops == nullptr) {
    return {error_domain::meta, errc::unsupported};
  }

  return register_capability_for_type<ops_type, value_type>(ops);
}

/** @brief 用调用点传入的 proxy 绑定生成 ops 并挂到运行期索引。内部入口。 */
template <typename Capability, typename T, typename... Bindings>
[[nodiscard]] error register_proxy_model_for() {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using model_type = proxy::model<capability_type, value_type,
                                  std::remove_cv_t<std::remove_reference_t<Bindings>>...>;
  using ops_type = typename model_type::ops_type;

  static_assert(std::is_same_v<ops_type, typename capability_type::ops_type>,
                "proxy model ops_type must match Capability::ops_type");

  const ops_type* ops = model_type::get();
  if (ops == nullptr) {
    return {error_domain::meta, errc::unsupported};
  }

  return register_capability_for_type<ops_type, value_type>(ops);
}

} // namespace detail

} // namespace caors::meta

#endif
