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

template <typename Capability, typename T, typename = void>
struct has_capability_auto_model : std::false_type {};

template <typename Capability, typename T>
struct has_capability_auto_model<
    Capability, T,
    std::void_t<typename std::remove_cv_t<std::remove_reference_t<Capability>>::template model<
                    std::remove_cv_t<std::remove_reference_t<T>>>::ops_type,
                decltype(std::remove_cv_t<std::remove_reference_t<Capability>>::template model<
                         std::remove_cv_t<std::remove_reference_t<T>>>::get())>> {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using model_type = typename capability_type::template model<value_type>;
  using ops_type = typename model_type::ops_type;

  static constexpr bool value = std::is_same_v<ops_type, typename capability_type::ops_type> &&
                                std::is_convertible_v<decltype(model_type::get()), const ops_type*>;
};

template <typename Capability, typename T>
inline constexpr bool has_capability_auto_model_v = has_capability_auto_model<Capability, T>::value;

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

/** @brief 把 capability 自身声明的自动 model 生成的 ops 挂到运行期索引。内部入口。 */
template <typename Capability, typename T>
[[nodiscard]] error register_capability_auto_model_for() {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using model_type = typename capability_type::template model<value_type>;
  using ops_type = typename model_type::ops_type;

  static_assert(std::is_same_v<ops_type, typename capability_type::ops_type>,
                "capability auto model ops_type must match Capability::ops_type");

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

#define CAORS_META_PROXY_DETAIL_CAT_IMPL(lhs, rhs) lhs##rhs
#define CAORS_META_PROXY_DETAIL_CAT(lhs, rhs) CAORS_META_PROXY_DETAIL_CAT_IMPL(lhs, rhs)

#define CAORS_META_PROXY_DETAIL_ARG_COUNT_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, count, ...) count
#define CAORS_META_PROXY_DETAIL_ARG_COUNT(...)                                                     \
  CAORS_META_PROXY_DETAIL_ARG_COUNT_IMPL(__VA_ARGS__ __VA_OPT__(, ) 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define CAORS_META_PROXY_DETAIL_PARAM_DECL(pair) CAORS_META_PROXY_DETAIL_PARAM_DECL_IMPL pair
#define CAORS_META_PROXY_DETAIL_PARAM_DECL_IMPL(type, name) type name

#define CAORS_META_PROXY_DETAIL_PARAM_NAME(pair) CAORS_META_PROXY_DETAIL_PARAM_NAME_IMPL pair
#define CAORS_META_PROXY_DETAIL_PARAM_NAME_IMPL(type, name) name

#define CAORS_META_PROXY_DETAIL_COMMA_MAP_0(map)
#define CAORS_META_PROXY_DETAIL_COMMA_MAP_1(map, value) map(value)
#define CAORS_META_PROXY_DETAIL_COMMA_MAP_2(map, value, ...)                                       \
  map(value), CAORS_META_PROXY_DETAIL_COMMA_MAP_1(map, __VA_ARGS__)
#define CAORS_META_PROXY_DETAIL_COMMA_MAP_3(map, value, ...)                                       \
  map(value), CAORS_META_PROXY_DETAIL_COMMA_MAP_2(map, __VA_ARGS__)
#define CAORS_META_PROXY_DETAIL_COMMA_MAP_4(map, value, ...)                                       \
  map(value), CAORS_META_PROXY_DETAIL_COMMA_MAP_3(map, __VA_ARGS__)
#define CAORS_META_PROXY_DETAIL_COMMA_MAP_5(map, value, ...)                                       \
  map(value), CAORS_META_PROXY_DETAIL_COMMA_MAP_4(map, __VA_ARGS__)
#define CAORS_META_PROXY_DETAIL_COMMA_MAP_6(map, value, ...)                                       \
  map(value), CAORS_META_PROXY_DETAIL_COMMA_MAP_5(map, __VA_ARGS__)
#define CAORS_META_PROXY_DETAIL_COMMA_MAP_7(map, value, ...)                                       \
  map(value), CAORS_META_PROXY_DETAIL_COMMA_MAP_6(map, __VA_ARGS__)
#define CAORS_META_PROXY_DETAIL_COMMA_MAP_8(map, value, ...)                                       \
  map(value), CAORS_META_PROXY_DETAIL_COMMA_MAP_7(map, __VA_ARGS__)

#define CAORS_META_PROXY_DETAIL_COMMA_MAP(map, ...)                                                \
  CAORS_META_PROXY_DETAIL_CAT(CAORS_META_PROXY_DETAIL_COMMA_MAP_,                                  \
                              CAORS_META_PROXY_DETAIL_ARG_COUNT(__VA_ARGS__))(map __VA_OPT__(, )   \
                                                                                  __VA_ARGS__)

#define CAORS_META_PROXY_DETAIL_PARAM_DECLS(...)                                                   \
  CAORS_META_PROXY_DETAIL_COMMA_MAP(CAORS_META_PROXY_DETAIL_PARAM_DECL, __VA_ARGS__)

#define CAORS_META_PROXY_DETAIL_PARAM_NAMES(...)                                                   \
  CAORS_META_PROXY_DETAIL_COMMA_MAP(CAORS_META_PROXY_DETAIL_PARAM_NAME, __VA_ARGS__)

#define CAORS_META_PROXY_DETAIL_CONST_OPS_FIELD(name, result_type, noex, ...)                      \
  result_type (*name)(const void* __VA_OPT__(, ) CAORS_META_PROXY_DETAIL_PARAM_DECLS(__VA_ARGS__)) \
      noex = nullptr;

#define CAORS_META_PROXY_DETAIL_MUTABLE_OPS_FIELD(name, result_type, noex, ...)                    \
  result_type (*name)(void* __VA_OPT__(, ) CAORS_META_PROXY_DETAIL_PARAM_DECLS(__VA_ARGS__))       \
      noex = nullptr;

#define CAORS_META_PROXY_DETAIL_TYPE_OPS_FIELD(name, result_type, noex, ...)                       \
  result_type (*name)(CAORS_META_PROXY_DETAIL_PARAM_DECLS(__VA_ARGS__)) noex = nullptr;

#define CAORS_META_PROXY_DETAIL_OPS_VALID(name, result_type, noex, ...) (name != nullptr) &&

#define CAORS_META_PROXY_DETAIL_CONST_VIEW_METHOD(name, result_type, noex, ...)                    \
  result_type name(CAORS_META_PROXY_DETAIL_PARAM_DECLS(__VA_ARGS__)) const noex {                  \
    if constexpr (std::is_void_v<result_type>) {                                                   \
      ops()->name(data() __VA_OPT__(, ) CAORS_META_PROXY_DETAIL_PARAM_NAMES(__VA_ARGS__));         \
    } else {                                                                                       \
      return ops()->name(data() __VA_OPT__(, ) CAORS_META_PROXY_DETAIL_PARAM_NAMES(__VA_ARGS__));  \
    }                                                                                              \
  }

#define CAORS_META_PROXY_DETAIL_MUTABLE_VIEW_METHOD(name, result_type, noex, ...)                  \
  result_type name(CAORS_META_PROXY_DETAIL_PARAM_DECLS(__VA_ARGS__)) const noex {                  \
    if constexpr (std::is_void_v<result_type>) {                                                   \
      ops()->name(mutable_data() __VA_OPT__(, ) CAORS_META_PROXY_DETAIL_PARAM_NAMES(__VA_ARGS__)); \
    } else {                                                                                       \
      return ops()->name(mutable_data() __VA_OPT__(, )                                             \
                             CAORS_META_PROXY_DETAIL_PARAM_NAMES(__VA_ARGS__));                    \
    }                                                                                              \
  }

#define CAORS_META_PROXY_DETAIL_TYPE_FACADE_METHOD(name, result_type, noex, ...)                   \
  result_type name(CAORS_META_PROXY_DETAIL_PARAM_DECLS(__VA_ARGS__)) const noex {                  \
    if constexpr (std::is_void_v<result_type>) {                                                   \
      if (valid()) {                                                                               \
        ops_->name(CAORS_META_PROXY_DETAIL_PARAM_NAMES(__VA_ARGS__));                              \
      }                                                                                            \
    } else {                                                                                       \
      return valid() ? ops_->name(CAORS_META_PROXY_DETAIL_PARAM_NAMES(__VA_ARGS__))                \
                     : result_type{};                                                              \
    }                                                                                              \
  }

#define CAORS_META_PROXY_DETAIL_MODEL_METHOD(name, result_type, noex, ...)                         \
  , caors::meta::proxy::method<&ops_type::name, &value_type::name>

/**
 * @brief 定义可绑定实例的 proxy capability。
 *
 * `methods` 需要接受两个宏参数：第一个用于只读方法，第二个用于可写方法。方法项格式为
 * `METHOD(name, result_type, noexcept_or_empty, (param_type, param_name)...)`。
 */
#define CAORS_META_PROXY_CAPABILITY(capability_name, methods)                                      \
  struct capability_name##_ops {                                                                   \
    methods(CAORS_META_PROXY_DETAIL_CONST_OPS_FIELD, CAORS_META_PROXY_DETAIL_MUTABLE_OPS_FIELD)    \
        [[nodiscard]] bool valid() const noexcept {                                                \
      return methods(CAORS_META_PROXY_DETAIL_OPS_VALID, CAORS_META_PROXY_DETAIL_OPS_VALID) true;   \
    }                                                                                              \
  };                                                                                               \
  class capability_name##_view final                                                               \
      : public caors::meta::capability_view_base<capability_name##_ops> {                          \
  public:                                                                                          \
    using base_type = caors::meta::capability_view_base<capability_name##_ops>;                    \
    using base_type::base_type;                                                                    \
                                                                                                   \
    capability_name##_view() noexcept = default;                                                   \
                                                                                                   \
    methods(CAORS_META_PROXY_DETAIL_CONST_VIEW_METHOD,                                             \
            CAORS_META_PROXY_DETAIL_MUTABLE_VIEW_METHOD)                                           \
  };                                                                                               \
  class capability_name final                                                                      \
      : public caors::meta::type_capability_base<capability_name##_ops, capability_name##_view> {  \
  public:                                                                                          \
    using base_type =                                                                              \
        caors::meta::type_capability_base<capability_name##_ops, capability_name##_view>;          \
    using ops_type = capability_name##_ops;                                                        \
    using view_type = capability_name##_view;                                                      \
    using base_type::base_type;                                                                    \
                                                                                                   \
    capability_name() noexcept = default;                                                          \
                                                                                                   \
    template <typename T> struct model {                                                           \
      using value_type = std::remove_cv_t<std::remove_reference_t<T>>;                             \
      using proxy_model =                                                                          \
          caors::meta::proxy::model<capability_name,                                               \
                                    value_type methods(CAORS_META_PROXY_DETAIL_MODEL_METHOD,       \
                                                       CAORS_META_PROXY_DETAIL_MODEL_METHOD)>;     \
      using ops_type = typename proxy_model::ops_type;                                             \
                                                                                                   \
      [[nodiscard]] static const ops_type* get() noexcept { return proxy_model::get(); }           \
    };                                                                                             \
  };

/**
 * @brief 定义纯类型级 proxy capability。
 *
 * `methods` 需要接受一个宏参数，方法项格式同 `CAORS_META_PROXY_CAPABILITY`。第一版只生成
 * `ops` 和类型级 facade；函数表仍通过现有 `.capability(ops)` 或后续 model 扩展注册。
 */
#define CAORS_META_PROXY_TYPE_CAPABILITY(capability_name, methods)                                 \
  struct capability_name##_ops {                                                                   \
    methods(CAORS_META_PROXY_DETAIL_TYPE_OPS_FIELD) [[nodiscard]] bool valid() const noexcept {    \
      return methods(CAORS_META_PROXY_DETAIL_OPS_VALID) true;                                      \
    }                                                                                              \
  };                                                                                               \
  class capability_name final : public caors::meta::type_capability_base<capability_name##_ops> {  \
  public:                                                                                          \
    using base_type = caors::meta::type_capability_base<capability_name##_ops>;                    \
    using ops_type = capability_name##_ops;                                                        \
    using base_type::base_type;                                                                    \
                                                                                                   \
    capability_name() noexcept = default;                                                          \
                                                                                                   \
    methods(CAORS_META_PROXY_DETAIL_TYPE_FACADE_METHOD)                                            \
  };

#endif
