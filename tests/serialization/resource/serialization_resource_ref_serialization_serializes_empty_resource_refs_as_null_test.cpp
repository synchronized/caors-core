// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;
TEST_CASE("serialization - serializes empty resource refs as null", "[serialization]") {
  require_resource_ref_actor_metadata();

  const resource_ref_actor actor{};
  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);

  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"texture":null})");

  resource_ref_actor decoded{caors::resource_ref<serialized_texture_resource>{texture_guid}};
  const caors::error status = caors::deserialize_json(decoded, R"({"texture":null})");

  REQUIRE(status.succeeded());
  CHECK_FALSE(decoded.texture);
  CHECK(decoded.texture.guid() == caors::resource_guid::nil());
  CHECK_FALSE(decoded.texture.resolved());
}
