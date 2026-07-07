// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips unique pointer vectors", "[serialization]") {
  require_unique_ptr_vector_actor_metadata();

  unique_ptr_vector_actor actor{};
  actor.values.push_back(std::make_unique<int>(7));
  actor.values.push_back(std::unique_ptr<int>{});
  actor.values.push_back(std::make_unique<int>(9));

  const caors::serialization::serialization_text_result text = caors::serialize_json(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"values":[7,null,9]})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* values =
      value.value.find_member(caors::string_name{"values"});
  REQUIRE(values != nullptr);
  REQUIRE(values->value.is_array());
  REQUIRE(values->value.as_array().size() == 3);
  CHECK(values->value.as_array()[0].as_i64() == 7);
  CHECK(values->value.as_array()[1].is_null());
  CHECK(values->value.as_array()[2].as_i64() == 9);

  unique_ptr_vector_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"values":[4,null,6]})");
  REQUIRE(result.succeeded());
  REQUIRE(decoded.values.size() == 3);
  REQUIRE(decoded.values[0] != nullptr);
  CHECK(*decoded.values[0] == 4);
  CHECK(decoded.values[1] == nullptr);
  REQUIRE(decoded.values[2] != nullptr);
  CHECK(*decoded.values[2] == 6);
}
