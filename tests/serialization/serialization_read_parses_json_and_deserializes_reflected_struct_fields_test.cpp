// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - parses json and deserializes reflected struct fields",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reader_actor>("caors.test.serialization.reader_actor")
          .field("id", &reader_actor::id)
          .field("health", &reader_actor::health)
          .field("active", &reader_actor::active)
          .field("name", &reader_actor::name)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<reader_actor>()));

  const caors::serialization::variant_result parsed =
      caors::parse_json(R"({"id":7,"health":95.5,"active":true,"name":"Ada\nLovelace"})");
  REQUIRE(parsed.succeeded());
  REQUIRE(parsed.value.is_object());
  const caors::value::variant_member* name = parsed.value.find_member(caors::string_name{"name"});
  REQUIRE(name != nullptr);
  REQUIRE(name->value.is_string());
  CHECK(name->value.as_string() == "Ada\nLovelace");

  reader_actor actor{};
  const caors::error result =
      caors::deserialize_json(actor, R"({"id":7,"health":95.5,"active":true,"name":"Ada"})");

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
  CHECK(actor.health == Catch::Approx(95.5));
  CHECK(actor.active);
  CHECK(actor.name == "Ada");
}
