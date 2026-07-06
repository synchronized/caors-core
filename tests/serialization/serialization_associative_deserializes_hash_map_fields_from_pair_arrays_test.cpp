// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - deserializes hash map fields from pair arrays", "[serialization]") {
  using labels_type = caors::hash_map<int, caors::string>;
  require_registered_type<labels_type>("caors.hash_map<int,caors.string>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<map_actor>("caors.test.serialization.map_actor")
          .field("labels", &map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<map_actor>()));

  const caors::serialization::variant_result parsed =
      caors::parse_json(R"({"labels":[[3,"three"]]})");
  REQUIRE(parsed.succeeded());

  map_actor actor{};
  const caors::error result = caors::deserialize(actor, parsed.value);

  REQUIRE(result.succeeded());
  REQUIRE(actor.labels.size() == 1);
  CHECK(actor.labels.at(3) == "three");
}
