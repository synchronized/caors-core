// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - class builder can attach to explicit meta type", "[serialization]") {
  const caors::meta::type type = caors::meta::type_<explicit_meta_serialization_actor>(
                                     "caors.test.serialization.explicit_meta_actor")
                                     .get();
  REQUIRE(type.valid());
  CHECK(caors::meta::find<explicit_meta_serialization_actor>().id() == type.id());

  REQUIRE(reflection_result_ok(caors::reflection::class_<explicit_meta_serialization_actor>(type)
                                   .field("id", &explicit_meta_serialization_actor::id)
                                   .commit()));

  const caors::error status = caors::serialization::class_(type).field("id").commit();
  REQUIRE(serialization_result_ok(status));

  const explicit_meta_serialization_actor actor{42};
  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"id":42})");
}
