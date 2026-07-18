// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_PROXY_H_
#define CAORS_META_PROXY_H_

#include "caors/meta/capability_facade.h"
#include "caors/meta/type.h"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace caors::meta {

template <typename Ops> class capability_view_base;
template <typename Ops, typename View> class type_capability_base;

/**
 * @brief C++ 类型到 capability ops 的声明式 model 扩展点。
 *
 * 默认没有 model。用户可以特化该模板，让 `meta::class_<T>().capability<Capability>()`
 * 自动注册对应 `ops`。
 */
template <typename Capability, typename T> struct capability_model {};

namespace proxy {

template <typename Dispatch, typename Signature> struct convention {
  using dispatch_type = Dispatch;
  using signature_type = Signature;
};

template <typename Dispatch, typename Signature> struct type_convention {
  using dispatch_type = Dispatch;
  using signature_type = Signature;
};

template <auto Function> struct free_function_dispatch {
  template <typename T> static constexpr auto member = Function;
};

template <typename Signature> struct convention_slot;

template <typename R, typename... Args> struct convention_slot<R(Args...)> {
  using const_function_type = R (*)(const void*, Args...);
  using mutable_function_type = R (*)(void*, Args...);

  const_function_type const_call = nullptr;
  mutable_function_type mutable_call = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return const_call != nullptr || mutable_call != nullptr;
  }
};

template <typename R, typename... Args> struct convention_slot<R(Args...) noexcept> {
  using const_function_type = R (*)(const void*, Args...) noexcept;
  using mutable_function_type = R (*)(void*, Args...) noexcept;

  const_function_type const_call = nullptr;
  mutable_function_type mutable_call = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return const_call != nullptr || mutable_call != nullptr;
  }
};

template <typename Signature> struct type_convention_slot;

template <typename R, typename... Args> struct type_convention_slot<R(Args...)> {
  using function_type = R (*)(Args...);

  function_type call = nullptr;

  [[nodiscard]] bool valid() const noexcept { return call != nullptr; }
};

template <typename R, typename... Args> struct type_convention_slot<R(Args...) noexcept> {
  using function_type = R (*)(Args...) noexcept;

  function_type call = nullptr;

  [[nodiscard]] bool valid() const noexcept { return call != nullptr; }
};

template <std::size_t Index, typename Dispatch, typename Signature> struct indexed_convention {
  static constexpr std::size_t index = Index;
  static constexpr bool is_noexcept = false;
  using dispatch_type = Dispatch;
  using signature_type = Signature;
};

template <std::size_t Index, typename Dispatch, typename R, typename... Args>
struct indexed_convention<Index, Dispatch, R(Args...) noexcept> {
  static constexpr std::size_t index = Index;
  static constexpr bool is_noexcept = true;
  using dispatch_type = Dispatch;
  using signature_type = R(Args...) noexcept;
};

template <std::size_t Index, typename Dispatch, typename Signature> struct indexed_type_convention {
  static constexpr std::size_t index = Index;
  static constexpr bool is_noexcept = false;
  using dispatch_type = Dispatch;
  using signature_type = Signature;
};

template <std::size_t Index, typename Dispatch, typename R, typename... Args>
struct indexed_type_convention<Index, Dispatch, R(Args...) noexcept> {
  static constexpr std::size_t index = Index;
  static constexpr bool is_noexcept = true;
  using dispatch_type = Dispatch;
  using signature_type = R(Args...) noexcept;
};

template <typename IndexedConvention> struct convention_slot_for;

template <std::size_t Index, typename Dispatch, typename Signature>
struct convention_slot_for<indexed_convention<Index, Dispatch, Signature>> {
  using type = convention_slot<Signature>;
};

template <std::size_t Index, typename Dispatch, typename Signature>
struct convention_slot_for<indexed_type_convention<Index, Dispatch, Signature>> {
  using type = type_convention_slot<Signature>;
};

template <typename... Conventions> class convention_ops {
public:
  using storage_type = std::tuple<typename convention_slot_for<Conventions>::type...>;

  template <std::size_t Index> using slot_type = std::tuple_element_t<Index, storage_type>;

  [[nodiscard]] bool valid() const noexcept {
    return std::apply([](const auto&... slots) { return (slots.valid() && ...); }, slots_);
  }

  template <std::size_t Index> [[nodiscard]] const auto& get() const noexcept {
    return std::get<Index>(slots_);
  }

  template <std::size_t Index> [[nodiscard]] auto& get() noexcept {
    return std::get<Index>(slots_);
  }

private:
  storage_type slots_{};
};

template <typename MemberPointer> struct member_function_pointer_traits;

template <typename Class, typename R, typename... Args>
struct member_function_pointer_traits<R (Class::*)(Args...)> {
  static constexpr bool is_const = false;
  static constexpr bool is_noexcept = false;
};

template <typename Class, typename R, typename... Args>
struct member_function_pointer_traits<R (Class::*)(Args...) const> {
  static constexpr bool is_const = true;
  static constexpr bool is_noexcept = false;
};

template <typename Class, typename R, typename... Args>
struct member_function_pointer_traits<R (Class::*)(Args...) noexcept> {
  static constexpr bool is_const = false;
  static constexpr bool is_noexcept = true;
};

template <typename Class, typename R, typename... Args>
struct member_function_pointer_traits<R (Class::*)(Args...) const noexcept> {
  static constexpr bool is_const = true;
  static constexpr bool is_noexcept = true;
};

template <typename IndexedConvention, typename T> struct convention_callback;
template <typename IndexedConvention, typename T> struct type_convention_callback;

template <std::size_t Index, typename Dispatch, typename R, typename... Args, typename T>
struct convention_callback<indexed_convention<Index, Dispatch, R(Args...)>, T> {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  static constexpr auto member = Dispatch::template member<value_type>;
  using member_traits = member_function_pointer_traits<std::remove_cv_t<decltype(member)>>;

  static_assert(std::is_member_function_pointer_v<decltype(member)>,
                "proxy convention dispatch requires a member function pointer");

  [[nodiscard]] static R call_const(const void* object, Args... args) {
    if constexpr (std::is_void_v<R>) {
      if (object != nullptr) {
        (static_cast<const value_type*>(object)->*member)(std::forward<Args>(args)...);
      }
    } else {
      if (object == nullptr) {
        return R{};
      }
      return (static_cast<const value_type*>(object)->*member)(std::forward<Args>(args)...);
    }
  }

  static R call_mutable(void* object, Args... args) {
    if constexpr (std::is_void_v<R>) {
      if (object != nullptr) {
        (static_cast<value_type*>(object)->*member)(std::forward<Args>(args)...);
      }
    } else {
      if (object == nullptr) {
        return R{};
      }
      return (static_cast<value_type*>(object)->*member)(std::forward<Args>(args)...);
    }
  }

  template <typename Slot> static void apply(Slot& slot) {
    if constexpr (member_traits::is_const) {
      slot.const_call = &call_const;
    } else {
      slot.mutable_call = &call_mutable;
    }
  }
};

template <std::size_t Index, typename Dispatch, typename R, typename... Args, typename T>
struct convention_callback<indexed_convention<Index, Dispatch, R(Args...) noexcept>, T> {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  static constexpr auto member = Dispatch::template member<value_type>;
  using member_traits = member_function_pointer_traits<std::remove_cv_t<decltype(member)>>;

  static_assert(std::is_member_function_pointer_v<decltype(member)>,
                "proxy convention dispatch requires a member function pointer");
  static_assert(member_traits::is_noexcept,
                "noexcept proxy convention requires a noexcept member function");

  [[nodiscard]] static R call_const(const void* object, Args... args) noexcept {
    if constexpr (std::is_void_v<R>) {
      if (object != nullptr) {
        (static_cast<const value_type*>(object)->*member)(std::forward<Args>(args)...);
      }
    } else {
      if (object == nullptr) {
        return R{};
      }
      return (static_cast<const value_type*>(object)->*member)(std::forward<Args>(args)...);
    }
  }

  static R call_mutable(void* object, Args... args) noexcept {
    if constexpr (std::is_void_v<R>) {
      if (object != nullptr) {
        (static_cast<value_type*>(object)->*member)(std::forward<Args>(args)...);
      }
    } else {
      if (object == nullptr) {
        return R{};
      }
      return (static_cast<value_type*>(object)->*member)(std::forward<Args>(args)...);
    }
  }

  template <typename Slot> static void apply(Slot& slot) {
    if constexpr (member_traits::is_const) {
      slot.const_call = &call_const;
    } else {
      slot.mutable_call = &call_mutable;
    }
  }
};

template <std::size_t Index, typename Dispatch, typename R, typename... Args, typename T>
struct type_convention_callback<indexed_type_convention<Index, Dispatch, R(Args...)>, T> {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using function_type = R (*)(Args...);

  static constexpr auto member = Dispatch::template member<value_type>;

  static_assert(std::is_convertible_v<std::remove_cv_t<decltype(member)>, function_type>,
                "proxy type convention dispatch requires a static function with a matching "
                "signature");

  template <typename Slot> static void apply(Slot& slot) { slot.call = member; }
};

template <std::size_t Index, typename Dispatch, typename R, typename... Args, typename T>
struct type_convention_callback<indexed_type_convention<Index, Dispatch, R(Args...) noexcept>, T> {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using function_type = R (*)(Args...) noexcept;

  static constexpr auto member = Dispatch::template member<value_type>;

  static_assert(std::is_convertible_v<std::remove_cv_t<decltype(member)>, function_type>,
                "noexcept proxy type convention requires a noexcept static function with a "
                "matching signature");

  template <typename Slot> static void apply(Slot& slot) { slot.call = member; }
};

template <typename IndexedConvention> struct is_indexed_type_convention : std::false_type {};

template <std::size_t Index, typename Dispatch, typename Signature>
struct is_indexed_type_convention<indexed_type_convention<Index, Dispatch, Signature>>
    : std::true_type {};

template <typename T, typename Ops, typename IndexedConvention> void apply_convention(Ops& ops) {
  if constexpr (is_indexed_type_convention<IndexedConvention>::value) {
    type_convention_callback<IndexedConvention, T>::apply(
        ops.template get<IndexedConvention::index>());
  } else {
    convention_callback<IndexedConvention, T>::apply(ops.template get<IndexedConvention::index>());
  }
}

template <typename T, typename Ops, typename... IndexedConventions>
void apply_conventions(Ops& ops) {
  (apply_convention<T, Ops, IndexedConventions>(ops), ...);
}

template <typename IndexedConvention, typename View> struct view_mixin_for {};

template <std::size_t Index, typename Dispatch, typename Signature, typename View>
struct view_mixin_for<indexed_convention<Index, Dispatch, Signature>, View>
    : Dispatch::template view_mixin<indexed_convention<Index, Dispatch, Signature>, View> {};

template <typename Ops, typename... IndexedConventions>
class convention_view
    : public meta::capability_view_base<Ops>,
      public view_mixin_for<IndexedConventions, convention_view<Ops, IndexedConventions...>>... {
public:
  using base_type = meta::capability_view_base<Ops>;
  using base_type::base_type;

  convention_view() noexcept = default;

  template <typename Convention, typename R, typename... Args>
  R invoke(Args&&... args) const noexcept(Convention::is_noexcept) {
    const Ops* ops = this->ops();
    if (ops == nullptr) {
      if constexpr (!std::is_void_v<R>) {
        return R{};
      }
    } else {
      const auto& slot = ops->template get<Convention::index>();
      if (slot.const_call != nullptr) {
        if constexpr (std::is_void_v<R>) {
          slot.const_call(this->data(), std::forward<Args>(args)...);
        } else {
          return slot.const_call(this->data(), std::forward<Args>(args)...);
        }
      } else if (slot.mutable_call != nullptr) {
        if constexpr (std::is_void_v<R>) {
          slot.mutable_call(this->mutable_data(), std::forward<Args>(args)...);
        } else {
          return slot.mutable_call(this->mutable_data(), std::forward<Args>(args)...);
        }
      }
    }

    if constexpr (!std::is_void_v<R>) {
      return R{};
    }
  }
};

template <typename Dispatch, typename IndexedConvention, typename Facade, typename = void>
struct type_facade_mixin_selector {};

template <typename Dispatch, typename IndexedConvention, typename Facade>
struct type_facade_mixin_selector<
    Dispatch, IndexedConvention, Facade,
    std::void_t<typename Dispatch::template facade_mixin<IndexedConvention, Facade>>>
    : Dispatch::template facade_mixin<IndexedConvention, Facade> {};

template <typename IndexedConvention, typename Facade> struct type_facade_mixin_for {};

template <std::size_t Index, typename Dispatch, typename Signature, typename Facade>
struct type_facade_mixin_for<indexed_type_convention<Index, Dispatch, Signature>, Facade>
    : type_facade_mixin_selector<Dispatch, indexed_type_convention<Index, Dispatch, Signature>,
                                 Facade> {};

template <typename Capability, typename T> struct convention_model {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  using ops_type = typename capability_type::ops_type;

  inline static const ops_type value = [] {
    ops_type ops{};
    capability_type::template apply_conventions<value_type>(ops);
    return ops;
  }();

  [[nodiscard]] static const ops_type* get() noexcept {
    return meta::detail::capability_ops_valid(&value) ? &value : nullptr;
  }
};

template <typename... IndexedConventions>
class convention_capability final
    : public meta::type_capability_base<
          convention_ops<IndexedConventions...>,
          convention_view<convention_ops<IndexedConventions...>, IndexedConventions...>>,
      public type_facade_mixin_for<IndexedConventions,
                                   convention_capability<IndexedConventions...>>... {
public:
  using ops_type = convention_ops<IndexedConventions...>;
  using view_type = convention_view<ops_type, IndexedConventions...>;
  using base_type = meta::type_capability_base<ops_type, view_type>;
  using base_type::base_type;

  convention_capability() noexcept = default;

  template <typename T> struct model : convention_model<convention_capability, T> {};

  template <typename T> static void apply_conventions(ops_type& ops) {
    caors::meta::proxy::apply_conventions<T, ops_type, IndexedConventions...>(ops);
  }

  template <typename Convention, typename R, typename... Args>
  R invoke_type(Args&&... args) const noexcept(Convention::is_noexcept) {
    if (!this->valid()) {
      if constexpr (!std::is_void_v<R>) {
        return R{};
      }
    } else {
      const auto& slot = this->ops_->template get<Convention::index>();
      if (slot.call != nullptr) {
        if constexpr (std::is_void_v<R>) {
          slot.call(std::forward<Args>(args)...);
        } else {
          return slot.call(std::forward<Args>(args)...);
        }
      }
    }

    if constexpr (!std::is_void_v<R>) {
      return R{};
    }
  }
};

template <typename... Conventions> struct capability_builder_state;

template <std::size_t Index, typename Convention> struct index_convention;

template <std::size_t Index, typename Dispatch, typename Signature>
struct index_convention<Index, convention<Dispatch, Signature>> {
  using type = indexed_convention<Index, Dispatch, Signature>;
};

template <std::size_t Index, typename Dispatch, typename Signature>
struct index_convention<Index, type_convention<Dispatch, Signature>> {
  using type = indexed_type_convention<Index, Dispatch, Signature>;
};

template <typename Sequence, typename... Conventions> struct build_convention_capability;

template <std::size_t... Indexes, typename... Conventions>
struct build_convention_capability<std::index_sequence<Indexes...>, Conventions...> {
  using type = convention_capability<typename index_convention<Indexes, Conventions>::type...>;
};

template <typename... Conventions> struct capability_builder_state {
  template <typename Dispatch, typename Signature>
  using add_convention = capability_builder_state<Conventions..., convention<Dispatch, Signature>>;

  template <typename Dispatch, typename Signature>
  using add_type_convention =
      capability_builder_state<Conventions..., type_convention<Dispatch, Signature>>;

  using build =
      typename build_convention_capability<std::make_index_sequence<sizeof...(Conventions)>,
                                           Conventions...>::type;
};

} // namespace proxy

/**
 * @brief Proxy 风格 capability 构建器。
 *
 * `add_convention<Dispatch, Signature>` 增加一个成员访问约定，`build` 生成可直接用于
 * `meta::class_<T>().capability<Capability>()` 和 `any_ref::get_capability<Capability>()`
 * 的 capability 类型。
 */
class capability_builder {
public:
  template <typename Dispatch, typename Signature>
  using add_convention = proxy::capability_builder_state<proxy::convention<Dispatch, Signature>>;

  template <typename Dispatch, typename Signature>
  using add_type_convention =
      proxy::capability_builder_state<proxy::type_convention<Dispatch, Signature>>;
};

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

/** @brief 把 `capability_model<Capability, T>` 生成的 ops 挂到注册类型数据。内部入口。 */
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

/** @brief 把 `capability_model<Capability, T>` 生成的 ops 挂到指定注册表。内部入口。 */
template <typename Capability, typename T>
[[nodiscard]] error register_capability_model_for(registry& registry) {
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

  return register_capability_for_type<ops_type, value_type>(registry, ops);
}

/** @brief 把 capability 自身声明的自动 model 生成的 ops 挂到注册类型数据。内部入口。 */
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

/** @brief 把 capability 自身声明的自动 model 生成的 ops 挂到指定注册表。内部入口。 */
template <typename Capability, typename T>
[[nodiscard]] error register_capability_auto_model_for(registry& registry) {
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

  return register_capability_for_type<ops_type, value_type>(registry, ops);
}

} // namespace detail

} // namespace caors::meta

#define CAORS_META_DEF_MEM_DISPATCH(dispatch_name, member_name)                                    \
  struct dispatch_name {                                                                           \
    template <typename T>                                                                          \
    static constexpr auto member = &std::remove_cv_t<std::remove_reference_t<T>>::member_name;     \
                                                                                                   \
    template <typename Convention, typename View> struct view_mixin;                               \
                                                                                                   \
    template <std::size_t Index, typename R, typename... Args, typename View>                      \
    struct view_mixin<caors::meta::proxy::indexed_convention<Index, dispatch_name, R(Args...)>,    \
                      View> {                                                                      \
      R member_name(Args... args) const {                                                          \
        return static_cast<const View*>(this)                                                      \
            ->template invoke<                                                                     \
                caors::meta::proxy::indexed_convention<Index, dispatch_name, R(Args...)>, R>(      \
                std::forward<Args>(args)...);                                                      \
      }                                                                                            \
    };                                                                                             \
                                                                                                   \
    template <std::size_t Index, typename R, typename... Args, typename View>                      \
    struct view_mixin<                                                                             \
        caors::meta::proxy::indexed_convention<Index, dispatch_name, R(Args...) noexcept>, View> { \
      R member_name(Args... args) const noexcept {                                                 \
        return static_cast<const View*>(this)                                                      \
            ->template invoke<                                                                     \
                caors::meta::proxy::indexed_convention<Index, dispatch_name, R(Args...) noexcept>, \
                R>(std::forward<Args>(args)...);                                                   \
      }                                                                                            \
    };                                                                                             \
  }

#define CAORS_META_DEF_TYPE_DISPATCH(dispatch_name, member_name)                                   \
  struct dispatch_name {                                                                           \
    template <typename T>                                                                          \
    static constexpr auto member = &std::remove_cv_t<std::remove_reference_t<T>>::member_name;     \
                                                                                                   \
    template <typename Convention, typename Facade> struct facade_mixin;                           \
                                                                                                   \
    template <std::size_t Index, typename R, typename... Args, typename Facade>                    \
    struct facade_mixin<                                                                           \
        caors::meta::proxy::indexed_type_convention<Index, dispatch_name, R(Args...)>, Facade> {   \
      R member_name(Args... args) const {                                                          \
        return static_cast<const Facade*>(this)                                                    \
            ->template invoke_type<                                                                \
                caors::meta::proxy::indexed_type_convention<Index, dispatch_name, R(Args...)>, R>( \
                std::forward<Args>(args)...);                                                      \
      }                                                                                            \
    };                                                                                             \
                                                                                                   \
    template <std::size_t Index, typename R, typename... Args, typename Facade>                    \
    struct facade_mixin<                                                                           \
        caors::meta::proxy::indexed_type_convention<Index, dispatch_name, R(Args...) noexcept>,    \
        Facade> {                                                                                  \
      R member_name(Args... args) const noexcept {                                                 \
        return static_cast<const Facade*>(this)                                                    \
            ->template invoke_type<caors::meta::proxy::indexed_type_convention<                    \
                                       Index, dispatch_name, R(Args...) noexcept>,                 \
                                   R>(std::forward<Args>(args)...);                                \
      }                                                                                            \
    };                                                                                             \
  }

#define CAORS_META_DEF_FREE_TYPE_DISPATCH(dispatch_name, member_name, function_value)              \
  struct dispatch_name {                                                                           \
    template <typename T> static constexpr auto member = function_value;                           \
                                                                                                   \
    template <typename Convention, typename Facade> struct facade_mixin;                           \
                                                                                                   \
    template <std::size_t Index, typename R, typename... Args, typename Facade>                    \
    struct facade_mixin<                                                                           \
        caors::meta::proxy::indexed_type_convention<Index, dispatch_name, R(Args...)>, Facade> {   \
      R member_name(Args... args) const {                                                          \
        return static_cast<const Facade*>(this)                                                    \
            ->template invoke_type<                                                                \
                caors::meta::proxy::indexed_type_convention<Index, dispatch_name, R(Args...)>, R>( \
                std::forward<Args>(args)...);                                                      \
      }                                                                                            \
    };                                                                                             \
                                                                                                   \
    template <std::size_t Index, typename R, typename... Args, typename Facade>                    \
    struct facade_mixin<                                                                           \
        caors::meta::proxy::indexed_type_convention<Index, dispatch_name, R(Args...) noexcept>,    \
        Facade> {                                                                                  \
      R member_name(Args... args) const noexcept {                                                 \
        return static_cast<const Facade*>(this)                                                    \
            ->template invoke_type<caors::meta::proxy::indexed_type_convention<                    \
                                       Index, dispatch_name, R(Args...) noexcept>,                 \
                                   R>(std::forward<Args>(args)...);                                \
      }                                                                                            \
    };                                                                                             \
  }

#endif
