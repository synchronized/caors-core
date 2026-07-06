// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - generic writer backend serializes fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<writer_actor>("caors.test.serialization.writer_actor")
          .field("id", &writer_actor::id)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<writer_actor>({
      caors::serialization::make_field("id", {}, false, 7),
  })));

  numeric_field_writer writer;
  const writer_actor actor{42};
  const caors::error result = caors::serialize(actor, writer);

  REQUIRE(result.succeeded());
  CHECK(writer.object_open);
  CHECK(writer.object_closed);
  CHECK(writer.last_field_id == 7);
  CHECK(writer.last_i64 == 42);
}
