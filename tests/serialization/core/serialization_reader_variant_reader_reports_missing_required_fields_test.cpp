// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader reports missing required fields", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_field_policy_actor>(
                                   "caors.test.serialization.reader_field_policy_actor")
                                   .field("id", &reader_field_policy_actor::id)
                                   .field("optional", &reader_field_policy_actor::optional)
                                   .field("required", &reader_field_policy_actor::required)
                                   .field("skipped", &reader_field_policy_actor::skipped)
                                   .commit()));

  caors::serialization::field_data id = caors::serialization::make_field("id");
  caors::serialization::field_data optional = caors::serialization::make_field("optional");
  caors::serialization::field_data required = caors::serialization::make_field("required");
  required.required = true;
  caors::serialization::field_data skipped = caors::serialization::make_field("skipped");
  skipped.skipped = true;

  REQUIRE(serialization_result_ok(caors::serialization::register_type<reader_field_policy_actor>({
      id,
      optional,
      required,
      skipped,
  })));

  reader_field_policy_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object({}));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::not_found);
}
