// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_registry_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - registry returns type and field views", "[reflection]") {
  const caors::error register_status = register_view_actor();
  REQUIRE((register_status == caors::error{} ||
           register_status == reflection_error(caors::errc::duplicate_reflection)));

  auto reflected = caors::reflection::find("caors.test.reflection.view_actor");
  REQUIRE(reflected);
  CHECK(reflected.valid());
  CHECK(reflected.id() == registered_type_id("caors.test.reflection.view_actor"));
  CHECK(reflected.name() == "caors.test.reflection.view_actor");
  CHECK(reflected.guid() == view_actor_guid);
  CHECK(reflected.field_count() == 2);

  auto id = reflected.field_at(0);
  REQUIRE(id);
  CHECK(id.valid());
  CHECK(id.name() == "id");
  CHECK(id.type_name() == "int");
  CHECK(id.value_type() == registered_type_id("int"));
  CHECK_FALSE(reflected.field_at(99));

  auto health = reflected.find_field("health");
  REQUIRE(health);
  CHECK(health.name() == "health");
  CHECK(health.kind() == caors::meta::type_kind::value);
  CHECK(health.wrapper_semantics() == caors::meta::wrapper_semantics::none);
  CHECK_FALSE(health.indirect());
  CHECK_FALSE(health.nullable());
  CHECK(health.writable());
  CHECK(health.offset() == offsetof(view_actor, health));
  CHECK(health.size() == sizeof(float));
  CHECK(health.alignment() == alignof(float));

  view_actor instance{1, 25.0F};
  CHECK(health.try_set<float>(&instance, 90.0F));
  CHECK(instance.health == 90.0F);

  caors::any health_value = health.get_value(&instance);
  REQUIRE(health_value.valid());
  const float* copied_health = health_value.try_get<float>();
  REQUIRE(copied_health != nullptr);
  CHECK(*copied_health == 90.0F);

  const int next_id = 7;
  CHECK(id.try_set_value(&instance, caors::any::from(next_id)));
  CHECK(instance.id == 7);
}
