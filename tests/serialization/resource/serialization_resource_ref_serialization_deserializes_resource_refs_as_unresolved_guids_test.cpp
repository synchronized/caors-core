// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;
TEST_CASE("serialization - deserializes resource refs as unresolved guids", "[serialization]") {
  require_resource_ref_actor_metadata();

  resource_ref_actor decoded{};
  const caors::error status =
      caors::deserialize_json(decoded, R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  REQUIRE(status.succeeded());
  CHECK(decoded.texture.guid() == texture_guid);
  CHECK_FALSE(decoded.texture.resolved());
  CHECK(decoded.texture.get() == nullptr);

  caors::resource_cache<serialized_texture_resource> cache;
  serialized_texture_resource* texture = cache.create(texture_guid);
  REQUIRE(texture != nullptr);

  REQUIRE(cache.resolve(decoded.texture));
  CHECK(decoded.texture.resolved());
  CHECK(decoded.texture.get() == texture);
}
