// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes hash set fields as arrays", "[serialization]") {
  using ids_type = caors::hash_set<int>;
  require_registered_type<ids_type>("caors.hash_set<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<set_actor>("caors.test.serialization.set_actor")
          .field("ids", &set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<set_actor>()));

  set_actor actor{};
  actor.ids.emplace(7);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"ids":[7]})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* ids = value.value.find_member(caors::string_name{"ids"});
  REQUIRE(ids != nullptr);
  REQUIRE(ids->value.is_array());
  REQUIRE(ids->value.as_array().size() == 1);
  CHECK(ids->value.as_array()[0].as_i64() == 7);
}
