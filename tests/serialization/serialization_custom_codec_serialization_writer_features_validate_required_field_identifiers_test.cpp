// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_custom_codec_test_support.h"

using namespace caors::tests::serialization;
TEST_CASE("serialization - writer features validate required field identifiers",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<numeric_writer_actor>(
                                   "caors.test.serialization.numeric_writer_actor")
                                   .field("id", &numeric_writer_actor::id)
                                   .commit()));
  REQUIRE(caors::serialization::register_type<numeric_writer_actor>(
              {
                  caors::serialization::make_field("id"),
              })
              .succeeded());

  numeric_field_writer writer;
  const numeric_writer_actor actor{5};
  const caors::error result = caors::serialize(actor, writer);

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
}
