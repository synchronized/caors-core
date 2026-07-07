// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects weak pointer deserialization", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<weak_ptr_actor>("caors.test.serialization.weak_ptr_actor")
          .field("value", &weak_ptr_actor::value, "std.weak_ptr<int>")
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<weak_ptr_actor>()));

  weak_ptr_actor actor{};
  const caors::error result = caors::deserialize_json(actor, R"({"value":7})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(actor.value.expired());
}
