// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_CAPABILITY_FACADE_H_
#define CAORS_META_VALUE_CAPABILITY_FACADE_H_

#include "caors/meta/type.h"
#include "caors/meta/value/any_ref.h"

#include <type_traits>

namespace caors::meta {

namespace detail {

template <typename Ops> [[nodiscard]] bool capability_ops_valid(const Ops* ops) noexcept {
  if (ops == nullptr) {
    return false;
  }

  if constexpr (requires(const Ops& value) { value.valid(); }) {
    return static_cast<bool>(ops->valid());
  } else {
    return true;
  }
}

template <typename View, typename = void> struct is_auto_capability_view : std::false_type {};

template <typename View>
struct is_auto_capability_view<
    View, std::void_t<typename View::ops_type,
                      std::enable_if_t<std::is_constructible_v<
                          View, meta::type, const typename View::ops_type*, any_ref>>>>
    : std::true_type {};

template <typename View>
inline constexpr bool is_auto_capability_view_v = is_auto_capability_view<View>::value;

} // namespace detail

/**
 * @brief 自定义 capability view 的通用基类。
 *
 * `Ops` 是类型擦除函数表。派生 view 只需要声明 `using ops_type = Ops`，并通过继承构造函数接收
 * `(meta::type, const Ops*, any_ref)`，即可自动接入 `any_ref::get_capability<View>()` 和
 * `meta::type::get_capability<View>()`。
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

/**
 * @brief 类型级 capability facade，负责把类型能力延后绑定到具体 `any_ref`。
 */
template <typename View> class type_capability_view {
public:
  using view_type = std::remove_cv_t<std::remove_reference_t<View>>;
  using ops_type = typename view_type::ops_type;

  type_capability_view() noexcept = default;

  explicit type_capability_view(meta::type type) noexcept
      : type_(type), ops_(type.get_raw_ops<ops_type>()) {}

  [[nodiscard]] bool valid() const noexcept { return type_ && detail::capability_ops_valid(ops_); }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }

  [[nodiscard]] view_type bind(any_ref source) const noexcept {
    if (!valid() || !source.valid() || source.get_type().id() != type_.id()) {
      return {};
    }

    return view_type{type_, ops_, source};
  }

private:
  meta::type type_{};
  const ops_type* ops_ = nullptr;
};

template <typename View>
struct type_capability_traits<View, std::enable_if_t<detail::is_auto_capability_view_v<View>>> {
  using result_type = type_capability_view<View>;
  using view_type = std::remove_cv_t<std::remove_reference_t<View>>;
  using ops_type = typename view_type::ops_type;

  [[nodiscard]] static result_type get(const type& source) noexcept { return result_type{source}; }

  [[nodiscard]] static const ops_type* get_raw(const type& source) noexcept {
    return type_capability_traits<ops_type>::get_raw(source);
  }

  [[nodiscard]] static bool has(const type& source) noexcept { return get(source).valid(); }
};

template <typename View>
struct capability_view_traits<View, std::enable_if_t<detail::is_auto_capability_view_v<View>>> {
  static constexpr bool supported = true;

  [[nodiscard]] static View bind(any_ref source) noexcept {
    if (!source.valid()) {
      return {};
    }

    return type_capability_view<View>{source.get_type()}.bind(source);
  }
};

} // namespace caors::meta

#endif
