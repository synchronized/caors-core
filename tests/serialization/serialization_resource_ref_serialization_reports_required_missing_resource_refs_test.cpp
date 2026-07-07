// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;

TEST_CASE("serialization - required resource ref resolution reports missing resources",
          "[serialization]") {
  require_resource_ref_actor_metadata();

  caors::resource_cache<serialized_texture_resource> cache;
  const caors::serialization::resource_resolver resolver =
      caors::serialization::make_resource_resolver(cache);

  caors::serialization::context context{};
  context.resource_resolver = &resolver;
  context.resource_resolution = caors::serialization::resource_ref_resolution::require_resolved;

  resource_ref_actor decoded{};
  const caors::error status = caors::deserialize_json(
      decoded, R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})", context);

  CHECK(status == caors::error{caors::error_domain::resources, caors::errc::not_found});
  CHECK(decoded.texture.guid() == texture_guid);
  CHECK_FALSE(decoded.texture.resolved());
}
