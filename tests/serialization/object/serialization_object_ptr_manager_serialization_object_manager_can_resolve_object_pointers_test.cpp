// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_ptr_manager_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ptr_manager;
TEST_CASE("serialization - object manager can resolve object pointers", "[serialization]") {
  require_object_ptr_actor_metadata();

  caors::objects::manager manager;
  object_ptr_target* target = manager.create<object_ptr_target>();
  REQUIRE(target != nullptr);

  const caors::object_id target_id = target->id();
  const caors::serialization::reference_resolver resolver =
      caors::serialization::make_reference_resolver(manager);
  caors::serialization::context context{};
  context.references.object_resolver = &resolver;
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{target}};

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(context, actor);
  REQUIRE(json.succeeded());
  const std::string expected_json = "{\"target\":" + std::to_string(target_id.value()) + "}";
  CHECK(std::string_view{json.value.data(), json.value.size()} == std::string_view{expected_json});

  object_ptr_actor decoded{};
  const caors::error result = caors::deserialize_json(context, decoded, json.value);
  REQUIRE(result.succeeded());
  CHECK(decoded.target.get() == target);

  REQUIRE(manager.destroy(target_id));

  object_ptr_actor missing{};
  const caors::error missing_result = caors::deserialize_json(context, missing, json.value);
  CHECK(missing_result.failed());
  CHECK(missing_result.value() == caors::errc::not_found);
}
