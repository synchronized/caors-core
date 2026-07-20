// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_BASE_H_
#define CAORS_OBJECT_BASE_H_

#include "caors/meta/object.h"
#include "caors/object/fwd.h"

namespace caors::objects {

namespace detail {

using meta::detail::object_type_guid_of;

template <typename T, typename Void = void>
using object_type_guid_traits = meta::detail::object_type_guid_traits<T, Void>;

template <typename T, typename Void = void>
using object_base_type_traits = meta::detail::object_base_type_traits<T, Void>;

} // namespace detail

template <typename Derived, typename Base = object>
using object_base = meta::object_base<Derived, Base>;

} // namespace caors::objects

#endif
