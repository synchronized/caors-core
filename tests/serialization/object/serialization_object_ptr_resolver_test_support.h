// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

namespace caors::tests::serialization::object_ptr_resolver {

inline void require_object_ptr_actor_metadata() {
  require_serializable_property_type<object_ptr_actor>("caors.test.serialization.object_ptr_actor",
                                                       "target", &object_ptr_actor::target);
}

} // namespace caors::tests::serialization::object_ptr_resolver
