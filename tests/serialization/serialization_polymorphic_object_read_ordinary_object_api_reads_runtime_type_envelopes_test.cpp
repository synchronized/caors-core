// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - ordinary object API reads runtime type envelopes", "[serialization]") {
  require_metadata();

  player source{};
  source.level = 11;
  source.name = "Ren";
  const actor& source_actor = source;

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(source_actor);
  REQUIRE(json.succeeded());

  player target{};
  actor& target_actor = target;
  const caors::error status = caors::deserialize_json(target_actor, json.value);
  REQUIRE(status.succeeded());
  CHECK(target.level == 11);
  CHECK(target.name == "Ren");

  actor wrong_target{};
  const caors::error wrong_status = caors::deserialize_json(wrong_target, json.value);
  CHECK(wrong_status.failed());
  CHECK(wrong_status.value() == caors::errc::invalid_type);
}
