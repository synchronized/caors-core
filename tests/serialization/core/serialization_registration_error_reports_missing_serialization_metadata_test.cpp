// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - reports missing serialization metadata", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reflected_only_actor>("caors.test.serialization.reflected_only")
          .field("id", &reflected_only_actor::id)
          .commit()));

  const reflected_only_actor actor{3};
  const caors::serialization::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::missing_type);
  CHECK(result.value.empty());
}
