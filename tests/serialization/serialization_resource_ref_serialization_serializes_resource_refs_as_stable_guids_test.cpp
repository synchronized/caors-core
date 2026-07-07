// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_resource_ref_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::resource_ref_codec;
TEST_CASE("serialization - serializes resource refs as stable guids", "[serialization]") {
  require_resource_ref_actor_metadata();

  const resource_ref_actor actor{caors::resource_ref<serialized_texture_resource>{texture_guid}};
  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);

  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::value::variant_member* texture =
      value.value.find_member(caors::string_name{"texture"});
  REQUIRE(texture != nullptr);
  REQUIRE(texture->value.is_string());
  CHECK(texture->value.as_string() == "6cd2da3d-b549-4756-8b8b-0c84293af410");
}
