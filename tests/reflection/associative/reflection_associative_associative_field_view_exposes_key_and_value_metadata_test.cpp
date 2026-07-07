// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

#include "caors/value/associative_entry_ref.h"
#include "caors/value/view_factory.h"

using namespace test_reflection;

TEST_CASE("reflection - associative field view exposes key and value metadata", "[reflection]") {
  REQUIRE(caors::reflection::class_<hash_map_holder>("caors.test.reflection.hash_map_holder")
              .field("values", &hash_map_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.hash_map_holder");
  REQUIRE(reflected);

  auto values = reflected.find_field("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::associative);
  CHECK(values.is_associative());
  CHECK(values.type_name() == "caors.hash_map<int,int>");
  CHECK(values.key_type() == registered_type_id("int"));
  CHECK(values.key_type_name() == "int");
  CHECK(values.value_type() == registered_type_id("int"));
  CHECK(values.mapped_type() == registered_type_id("int"));
  CHECK(values.mapped_type_name() == "int");
}
