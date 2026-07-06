// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader reports missing fields", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_missing_field_actor>(
                                   "caors.test.serialization.reader_missing_field_actor")
                                   .field("id", &reader_missing_field_actor::id)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<reader_missing_field_actor>()));

  reader_missing_field_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object({}));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::not_found);
}
