// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;

TEST_CASE("serialization - resource ref can resolve through context resolver", "[serialization]") {
  require_resource_ref_actor_metadata();

  caors::resource_cache<serialized_texture_resource> cache;
  serialized_texture_resource* texture = cache.create(texture_guid);
  REQUIRE(texture != nullptr);
  caors::resource_cache<serialized_texture_resource> missing_cache;

  const caors::serialization::resource_resolver resolver =
      caors::serialization::make_resource_resolver(cache);
  const caors::serialization::resource_resolver missing_resolver =
      caors::serialization::make_resource_resolver(missing_cache);

  caors::serialization::context context{};
  context.resources.resolver = &resolver;
  context.resources.resolution = caors::serialization::resource_ref_resolution::resolve_if_present;

  resource_ref_actor decoded{};
  const caors::error status = caors::deserialize_json(
      context, decoded, R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  REQUIRE(status.succeeded());
  CHECK(decoded.texture.guid() == texture_guid);
  CHECK(decoded.texture.resolved());
  CHECK(decoded.texture.get() == texture);

  caors::serialization::context unresolved_context{};
  unresolved_context.resources.resolver = &missing_resolver;
  unresolved_context.resources.resolution =
      caors::serialization::resource_ref_resolution::keep_unresolved;

  resource_ref_actor unresolved{};
  const caors::error unresolved_status = caors::deserialize_json(
      unresolved_context, unresolved, R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  REQUIRE(unresolved_status.succeeded());
  CHECK(unresolved.texture.guid() == texture_guid);
  CHECK_FALSE(unresolved.texture.resolved());
}
