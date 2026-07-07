// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - ordinary object API writes runtime type envelopes", "[serialization]") {
  require_metadata();

  player player_value{};
  player_value.level = 8;
  player_value.name = "Ira";
  const actor& actor_value = player_value;

  const caors::serialization::variant_result value = caors::to_serialization_value(actor_value);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::value::variant_member* type_member =
      value.value.find_member(caors::string_name{"$type"});
  REQUIRE(type_member != nullptr);
  REQUIRE(type_member->value.is_string());
  CHECK(std::string_view{type_member->value.as_string().data(),
                         type_member->value.as_string().size()} ==
        std::string_view{player::object_type_guid.value.string()});

  const caors::value::variant_member* stored_value =
      value.value.find_member(caors::string_name{"$value"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());
  CHECK(stored_value->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored_value->value.find_member(caors::string_name{"name"}) != nullptr);

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(actor_value);
  REQUIRE(json.succeeded());
  CHECK(json.value.find("$type") != caors::string::npos);
  CHECK(json.value.find("$value") != caors::string::npos);
  CHECK(json.value.find("level") != caors::string::npos);
}
