// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_CALLABLE_SIGNATURE_H_
#define CAORS_META_DETAIL_CALLABLE_SIGNATURE_H_

#include <type_traits>

namespace caors::meta::detail {

template <typename Return, typename... Arguments> struct function_signature {
  using return_type = Return;
};

template <typename Signature> struct callable_signature;

template <typename Class, typename Return, typename... Arguments>
struct callable_signature<Return (Class::*)(Arguments...) const> {
  using type = function_signature<Return, Arguments...>;
};

template <typename Class, typename Return, typename... Arguments>
struct callable_signature<Return (Class::*)(Arguments...) const noexcept> {
  using type = function_signature<Return, Arguments...>;
};

template <typename Class, typename Return, typename... Arguments>
struct callable_signature<Return (Class::*)(Arguments...)> {
  using type = function_signature<Return, Arguments...>;
};

template <typename Class, typename Return, typename... Arguments>
struct callable_signature<Return (Class::*)(Arguments...) noexcept> {
  using type = function_signature<Return, Arguments...>;
};

template <typename Callable>
using callable_signature_t =
    typename callable_signature<decltype(&std::decay_t<Callable>::operator())>::type;

} // namespace caors::meta::detail

#endif
