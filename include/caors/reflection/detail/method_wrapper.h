// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_DETAIL_METHOD_WRAPPER_H_
#define CAORS_REFLECTION_DETAIL_METHOD_WRAPPER_H_

#include "caors/meta/detail/method_data.h"

namespace caors::reflection::detail {

using method_ops = meta::detail::method_ops;
using method_wrapper = meta::detail::method_data;

using meta::detail::const_member_method_ops;
using meta::detail::member_method_ops;

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_wrapper make_method_wrapper(std::string_view name,
                                                 Return (Class::*member)(Arguments...)) {
  return meta::detail::make_method_data<Class>(name, member);
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_wrapper make_method_wrapper(std::string_view name,
                                                 Return (Class::*member)(Arguments...) const) {
  return meta::detail::make_method_data<Class>(name, member);
}

} // namespace caors::reflection::detail

#endif
