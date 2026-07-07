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
  REQUIRE(type_result_ok(
      caors::meta::type_<context_resource_value>("caors.test.serialization.context_resource_value")
          .result()));
  REQUIRE(
      type_result_ok(caors::meta::type_<caors::memory::unique_ptr<context_resource_value>>(
                         "caors.memory.unique_ptr<caors.test.serialization.context_resource_value>")
                         .result()));
  REQUIRE(type_result_ok(caors::meta::type_<std::shared_ptr<context_resource_value>>(
                             "std.shared_ptr<caors.test.serialization.context_resource_value>")
                             .result()));
  REQUIRE(type_result_ok(
      caors::meta::type_<std::vector<caors::string>>("std.vector<caors.string>").result()));
  REQUIRE(type_result_ok(
      caors::meta::type_<std::optional<caors::string>>("std.optional<caors.string>").result()));
  REQUIRE(type_result_ok(
      caors::meta::type_<std::variant<int, caors::string>>("std.variant<int,caors.string>")
          .result()));

  REQUIRE(reflection_result_ok(caors::reflection::class_<context_resource_value>(
                                   "caors.test.serialization.context_resource_value")
                                   .field("value", &context_resource_value::value)
                                   .commit()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<context_resource_holder>(
          "caors.test.serialization.context_resource_holder")
          .field("unique", &context_resource_holder::unique,
                 "caors.memory.unique_ptr<caors.test.serialization.context_resource_value>")
          .field("shared", &context_resource_holder::shared,
                 "std.shared_ptr<caors.test.serialization.context_resource_value>")
          .field("names", &context_resource_holder::names, "std.vector<caors.string>")
          .field("maybe", &context_resource_holder::maybe, "std.optional<caors.string>")
          .field("choice", &context_resource_holder::choice, "std.variant<int,caors.string>")
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<context_resource_value>()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<context_resource_holder>()));
}

} // namespace caors::tests::serialization::descriptor_resource
