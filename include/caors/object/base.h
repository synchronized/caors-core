// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_BASE_H_
#define CAORS_OBJECT_BASE_H_

#include "caors/meta/object.h"
#include "caors/object/fwd.h"

namespace caors::objects {

template <typename Derived, typename Base = object>
using object_base = meta::object_base<Derived, Base>;

} // namespace caors::objects

#endif
