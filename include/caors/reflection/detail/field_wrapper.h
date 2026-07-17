// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_DETAIL_FIELD_WRAPPER_H_
#define CAORS_REFLECTION_DETAIL_FIELD_WRAPPER_H_

#include "caors/meta/detail/field_data.h"

namespace caors::reflection::detail {

using field_ops = meta::detail::field_ops;
using field_wrapper = meta::detail::field_data;

using meta::detail::field_type_matches_name;
using meta::detail::member_field_ops;
using meta::detail::member_offset;

template <typename Class, typename Field>
[[nodiscard]] field_wrapper make_field_wrapper(std::string_view name, std::string_view type_name,
                                               std::size_t offset) {
  return meta::detail::make_field_data<Class, Field>(name, type_name, offset);
}

} // namespace caors::reflection::detail

#endif
