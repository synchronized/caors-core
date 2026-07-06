// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - recursively serializes codec values inside vectors", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<guid_vector_actor>("caors.test.serialization.guid_vector_actor")
          .field("ids", &guid_vector_actor::ids, "caors.vector<guid>")
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<guid_vector_actor>()));

  const std::optional<caors::guid> first =
      caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  const std::optional<caors::guid> second =
      caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());

  guid_vector_actor actor{};
  actor.ids.push_back(*first);
  actor.ids.push_back(*second);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(
      text.value ==
      R"({"ids":["2f6d1f64-7a3a-4db2-9a2d-123456789abc","5ed39a97-7b20-4d87-86ac-e62a4a615b10"]})");

  guid_vector_actor decoded{};
  const caors::error result = caors::deserialize_json(
      decoded,
      R"({"ids":["5ed39a97-7b20-4d87-86ac-e62a4a615b10","2f6d1f64-7a3a-4db2-9a2d-123456789abc"]})");

  REQUIRE(result.succeeded());
  REQUIRE(decoded.ids.size() == 2);
  CHECK(decoded.ids[0] == *second);
  CHECK(decoded.ids[1] == *first);
}
