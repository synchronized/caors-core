// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - polymorphic object envelope names are configurable", "[serialization]") {
  require_metadata();

  caors::serialization::context context{};
  context.polymorphic.type_field = caors::string_name{"kind"};
  context.polymorphic.value_field = caors::string_name{"data"};
  context.polymorphic.type_name_field = caors::string_name{"debug_type"};
  context.polymorphic.write_type_name = true;

  player player_value{};
  player_value.level = 13;
  player_value.name = "Kai";
  const actor& actor_value = player_value;

  const caors::serialization::variant_result value =
      caors::to_serialization_value(actor_value, context);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  CHECK(value.value.find_member(caors::string_name{"$type"}) == nullptr);
  CHECK(value.value.find_member(caors::string_name{"$value"}) == nullptr);

  const caors::value::variant_member* type_member =
      value.value.find_member(caors::string_name{"kind"});
  REQUIRE(type_member != nullptr);
  REQUIRE(type_member->value.is_string());
  CHECK(std::string_view{type_member->value.as_string().data(),
                         type_member->value.as_string().size()} ==
        std::string_view{player::object_type_guid.value.string()});

  const caors::value::variant_member* type_name_member =
      value.value.find_member(caors::string_name{"debug_type"});
  REQUIRE(type_name_member != nullptr);
  REQUIRE(type_name_member->value.is_string());
  CHECK(std::string_view{type_name_member->value.as_string().data(),
                         type_name_member->value.as_string().size()} == player::object_type_name);

  const caors::value::variant_member* stored_value =
      value.value.find_member(caors::string_name{"data"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());
  CHECK(stored_value->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored_value->value.find_member(caors::string_name{"name"}) != nullptr);

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(actor_value, context);
  REQUIRE(json.succeeded());
  CHECK(json.value.find("kind") != caors::string::npos);
  CHECK(json.value.find("data") != caors::string::npos);
  CHECK(json.value.find("debug_type") != caors::string::npos);

  player target{};
  actor& target_actor = target;
  const caors::error status = caors::deserialize_json(target_actor, json.value, context);
  REQUIRE(status.succeeded());
  CHECK(target.level == 13);
  CHECK(target.name == "Kai");
}
