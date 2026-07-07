// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;

TEST_CASE("serialization - resource refs are collected as dependencies", "[serialization]") {
  require_resource_ref_actor_metadata();

  caors::serialization::resource_dependency_list dependencies;
  caors::serialization::resource_dependency_collector collector = dependencies.collector();

  caors::serialization::context context{};
  context.resources.dependencies = &collector;

  const resource_ref_actor actor{caors::resource_ref<serialized_texture_resource>{texture_guid}};
  const caors::serialization::serialization_text_result json =
      caors::serialize_json(actor, context);

  REQUIRE(json.succeeded());
  REQUIRE(dependencies.size() == 1);
  CHECK(dependencies.entries()[0].guid == texture_guid);
  CHECK(dependencies.entries()[0].type == caors::meta::id_of<serialized_texture_resource>());

  dependencies.clear();
  resource_ref_actor decoded{};
  const caors::error status = caors::deserialize_json(
      decoded, R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})", context);

  REQUIRE(status.succeeded());
  REQUIRE(dependencies.size() == 1);
  CHECK(dependencies.entries()[0].guid == texture_guid);
  CHECK(dependencies.entries()[0].type == caors::meta::id_of<serialized_texture_resource>());
}
