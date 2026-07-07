// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes std span fields as borrowed views", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<span_actor>("caors.test.serialization.span_actor")
          .field("scores", &span_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<span_actor>()));

  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");
}
