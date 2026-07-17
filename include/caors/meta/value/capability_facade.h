// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_CAPABILITY_FACADE_H_
#define CAORS_META_VALUE_CAPABILITY_FACADE_H_

#include "caors/meta/capability_facade.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any_ref.h"

#include <type_traits>
#include <utility>

namespace caors::meta {

namespace detail {

template <typename View, typename = void> struct is_auto_capability_view : std::false_type {};

template <typename View>
struct is_auto_capability_view<
    View, std::void_t<typename View::ops_type,
                      std::enable_if_t<std::is_constructible_v<
                          View, meta::type, const typename View::ops_type*, any_ref>>>>
    : std::true_type {};

template <typename View>
inline constexpr bool is_auto_capability_view_v = is_auto_capability_view<View>::value;

template <typename Capability, typename = void>
struct is_bindable_type_capability : std::false_type {};

template <typename Capability>
struct is_bindable_type_capability<
    Capability,
    std::void_t<typename std::remove_cv_t<std::remove_reference_t<Capability>>::view_type,
                decltype(std::declval<std::remove_cv_t<std::remove_reference_t<Capability>>>().bind(
                    std::declval<any_ref>()))>> {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using view_type = typename capability_type::view_type;
  using bind_result = decltype(std::declval<capability_type>().bind(std::declval<any_ref>()));

  static constexpr bool value =
      is_auto_type_capability_v<capability_type> && std::is_convertible_v<bind_result, view_type>;
};

template <typename Capability>
inline constexpr bool is_bindable_type_capability_v =
    is_bindable_type_capability<Capability>::value;

} // namespace detail

/**
 * @brief capability 绑定策略。
 *
 * 自定义 view 如果继承 `capability_view_base<Ops>`，默认策略会直接使用
 * `(meta::type, const Ops*, any_ref)` 构造 view。现有内置 view 可以特化该模板，复用自己的
 * `make_*_view()` 工厂。
 */
template <typename View, typename = void> struct capability_bind_traits {
  using view_type = std::remove_cv_t<std::remove_reference_t<View>>;

  static constexpr bool supported = false;
};

/**
 * @brief 自定义 capability view 的通用基类。
 *
 * `Ops` 是类型擦除函数表。派生 view 只需要声明 `using ops_type = Ops`，并通过继承构造函数接收
 * `(meta::type, const Ops*, any_ref)`，即可作为 `type_capability_base<Ops, View>` 的绑定结果；
 * 实例查询应通过对应 capability facade，例如 `any_ref::get_capability<MyCapability>()`。
 */
template <typename Ops> class capability_view_base {
public:
  using ops_type = std::remove_cv_t<std::remove_reference_t<Ops>>;

  capability_view_base() noexcept = default;

  capability_view_base(meta::type type, const ops_type* ops, any_ref source) noexcept
      : type_(type), ops_(ops), source_(source) {}

  [[nodiscard]] bool valid() const noexcept {
    return type_ && source_.valid() && source_.get_type().id() == type_.id() &&
           detail::capability_ops_valid(ops_);
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }

  [[nodiscard]] bool writable() const noexcept { return valid() && source_.writable(); }

protected:
  [[nodiscard]] const ops_type* ops() const noexcept { return valid() ? ops_ : nullptr; }

  [[nodiscard]] any_ref source() const noexcept { return valid() ? source_ : any_ref{}; }

  [[nodiscard]] const void* data() const noexcept { return valid() ? source_.data() : nullptr; }

  [[nodiscard]] void* mutable_data() const noexcept {
    return writable() ? source_.mutable_data() : nullptr;
  }

private:
  meta::type type_{};
  const ops_type* ops_ = nullptr;
  any_ref source_{};
};

template <typename View>
struct capability_bind_traits<View, std::enable_if_t<detail::is_auto_capability_view_v<View>>> {
  using view_type = std::remove_cv_t<std::remove_reference_t<View>>;
  using ops_type = typename view_type::ops_type;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type type, const ops_type* ops,
                                      any_ref source) noexcept {
    if (!type || !source.valid() || source.get_type().id() != type.id() ||
        !detail::capability_ops_valid(ops)) {
      return {};
    }

    return view_type{type, ops, source};
  }
};

template <typename Ops, typename View>
[[nodiscard]] typename type_capability_base<Ops, View>::view_type
type_capability_base<Ops, View>::bind(any_ref source) const {
  static_assert(capability_bind_traits<view_type>::supported,
                "Unsupported CAORS capability bind view");
  return capability_bind_traits<view_type>::bind(this->type_, this->ops_, source);
}

template <typename Capability>
struct capability_view_traits<Capability,
                              std::enable_if_t<detail::is_bindable_type_capability_v<Capability>>> {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using result_type = typename capability_type::view_type;

  static constexpr bool supported = true;

  [[nodiscard]] static result_type bind(any_ref source) {
    if (!source.valid()) {
      return {};
    }

    return capability_type{source.get_type()}.bind(source);
  }
};

} // namespace caors::meta

#endif
