// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects std span deserialization by default", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<span_actor>("caors.test.serialization.span_actor")
          .field("scores", &span_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<span_actor>()));

  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};

  const caors::error result = caors::deserialize_json(actor, R"({"scores":[5,6,7]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(storage == std::array<int, 3>{1, 2, 3});
}
