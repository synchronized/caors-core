// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - recursively serializes nested vector values", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<nested_vector_values_actor>(
                                   "caors.test.serialization.nested_vector_values_actor")
                                   .field("groups", &nested_vector_values_actor::groups)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<nested_vector_values_actor>()));

  nested_vector_values_actor actor{};
  actor.groups.push_back(caors::vector<int>{1, 2});
  actor.groups.push_back(caors::vector<int>{3});

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"groups":[[1,2],[3]]})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* groups =
      value.value.find_member(caors::string_name{"groups"});
  REQUIRE(groups != nullptr);
  REQUIRE(groups->value.is_array());
  REQUIRE(groups->value.as_array().size() == 2);
  CHECK(groups->value.as_array()[0].as_array()[1].as_i64() == 2);

  nested_vector_values_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"groups":[[4,5],[6]]})");
  REQUIRE(result.succeeded());
  REQUIRE(decoded.groups.size() == 2);
  REQUIRE(decoded.groups[0].size() == 2);
  CHECK(decoded.groups[0][0] == 4);
  CHECK(decoded.groups[0][1] == 5);
  REQUIRE(decoded.groups[1].size() == 1);
  CHECK(decoded.groups[1][0] == 6);
}
