// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - reports unsupported weak pointer fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<weak_ptr_actor>("caors.test.serialization.weak_ptr_actor")
          .field("value", &weak_ptr_actor::value)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<weak_ptr_actor>()));

  const std::shared_ptr<int> value = std::make_shared<int>(7);
  const weak_ptr_actor actor{value};
  const caors::serialization::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::unsupported);
}
