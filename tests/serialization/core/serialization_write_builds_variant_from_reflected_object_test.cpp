// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - builds variant from reflected object", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<dom_actor>("caors.test.serialization.dom_actor")
          .field("id", &dom_actor::id)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<dom_actor>({
      caors::serialization::make_field("id", "actor_id", false, 7),
  })));

  const dom_actor actor{42};
  const caors::serialization::variant_result result = caors::to_serialization_value(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value.kind() == caors::value::variant_kind::object);
  REQUIRE(result.value.as_object().size() == 1);
  CHECK(result.value.as_object()[0].name == "actor_id");
  CHECK(result.value.as_object()[0].field_id == 7);
  CHECK(result.value.as_object()[0].value.kind() == caors::value::variant_kind::signed_integer);
  CHECK(result.value.as_object()[0].value.as_i64() == 42);
}
