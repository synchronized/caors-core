// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips char and enum fields", "[serialization]") {
  require_scalar_actor_metadata();

  scalar_actor actor{};
  actor.code = 'A';
  actor.color = scalar_color::blue;
  actor.state = scalar_state::enabled;

  const caors::serialization::serialization_text_result text = caors::serialize_json(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"code":"A","color":2,"state":2})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());

  const caors::value::variant_member* code = value.value.find_member(caors::string_name{"code"});
  const caors::value::variant_member* color = value.value.find_member(caors::string_name{"color"});
  const caors::value::variant_member* state = value.value.find_member(caors::string_name{"state"});
  REQUIRE(code != nullptr);
  REQUIRE(color != nullptr);
  REQUIRE(state != nullptr);
  REQUIRE(code->value.try_as_string() != nullptr);
  REQUIRE(color->value.try_as_u64() != nullptr);
  REQUIRE(state->value.try_as_i64() != nullptr);
  CHECK(code->value.as_string() == "A");
  CHECK(color->value.as_u64() == 2U);
  CHECK(state->value.as_i64() == 2);

  scalar_actor decoded{};
  const caors::error result =
      caors::deserialize_json(decoded, R"({"code":"Z","color":1,"state":-1})");
  REQUIRE(result.succeeded());
  CHECK(decoded.code == 'Z');
  CHECK(decoded.color == scalar_color::red);
  CHECK(decoded.state == scalar_state::disabled);
}
