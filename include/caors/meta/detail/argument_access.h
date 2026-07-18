// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_ARGUMENT_ACCESS_H_
#define CAORS_META_DETAIL_ARGUMENT_ACCESS_H_

#include "caors/meta/argument.h"

#include <span>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

template <typename T> using argument_value_type = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename Argument> struct call_argument_supported {
  static constexpr bool value = std::is_lvalue_reference_v<Argument> ||
                                std::is_move_constructible_v<argument_value_type<Argument>>;
};

template <typename... Arguments>
inline constexpr bool call_arguments_supported_v =
    (call_argument_supported<Arguments>::value && ...);

template <typename Parameter> [[nodiscard]] bool argument_matches(const argument& value) noexcept {
  using value_type = argument_value_type<Parameter>;
  if (!value.template matches_type<value_type>()) {
    return false;
  }

  if constexpr (std::is_rvalue_reference_v<Parameter>) {
    return value.rvalue() && value.writable();
  } else if constexpr (std::is_lvalue_reference_v<Parameter> &&
                       !std::is_const_v<std::remove_reference_t<Parameter>>) {
    return value.writable();
  } else {
    return true;
  }
}

template <typename Parameter>
using argument_pass_type =
    std::conditional_t<std::is_rvalue_reference_v<Parameter>, argument_value_type<Parameter>&&,
                       std::conditional_t<std::is_lvalue_reference_v<Parameter> &&
                                              !std::is_const_v<std::remove_reference_t<Parameter>>,
                                          argument_value_type<Parameter>&,
                                          std::conditional_t<std::is_lvalue_reference_v<Parameter>,
                                                             const argument_value_type<Parameter>&,
                                                             argument_value_type<Parameter>>>>;

template <typename Parameter>
argument_pass_type<Parameter> argument_value(const argument& value) noexcept {
  using value_type = argument_value_type<Parameter>;
  if constexpr (std::is_rvalue_reference_v<Parameter>) {
    auto* result = value.template try_get_mutable<value_type>();
    value.consume();
    return std::move(*result);
  } else if constexpr (std::is_lvalue_reference_v<Parameter> &&
                       !std::is_const_v<std::remove_reference_t<Parameter>>) {
    return *value.template try_get_mutable<value_type>();
  } else if constexpr (!std::is_reference_v<Parameter>) {
    if (value.rvalue()) {
      auto* result = value.template try_get_mutable<value_type>();
      value.consume();
      return std::move(*result);
    }

    return *value.template try_get<value_type>();
  } else {
    return *value.template try_get<value_type>();
  }
}

template <typename... Arguments>
[[nodiscard]] bool arguments_match(std::span<const argument> arguments) noexcept {
  if (arguments.size() != sizeof...(Arguments)) {
    return false;
  }

  std::size_t index = 0;
  return ((argument_matches<Arguments>(arguments[index++])) && ...);
}

} // namespace caors::meta::detail

#endif
