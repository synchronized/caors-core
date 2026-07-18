// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

#include "caors/memory.h"

#include <vector>

namespace caors::tests::serialization::descriptor_resource {

enum class descriptor_enum_color : unsigned char {
  red = 1,
  blue = 2,
};

struct descriptor_enum_actor {
  std::vector<descriptor_enum_color> colors{};
};

struct context_resource_value {
  int value = 0;
};

struct context_resource_holder {
  caors::memory::unique_ptr<context_resource_value> unique{};
  std::shared_ptr<context_resource_value> shared{};
  std::vector<caors::string> names{};
  std::optional<caors::string> maybe{};
  std::variant<int, caors::string> choice{0};
};

inline void require_context_resource_holder_metadata() {
  require_serializable_property_type<context_resource_value>(
      "caors.test.serialization.context_resource_value", "value", &context_resource_value::value);
  REQUIRE(registration_result_ok(caors::registration::type_<context_resource_holder>(
                                     "caors.test.serialization.context_resource_holder")
                                     .meta()
                                     .property("unique", &context_resource_holder::unique)
                                     .property("shared", &context_resource_holder::shared)
                                     .property("names", &context_resource_holder::names)
                                     .property("maybe", &context_resource_holder::maybe)
                                     .property("choice", &context_resource_holder::choice)
                                     .serialization()
                                     .commit()));
}

} // namespace caors::tests::serialization::descriptor_resource
