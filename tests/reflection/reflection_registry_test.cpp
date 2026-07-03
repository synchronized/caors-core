// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

namespace {

[[nodiscard]] caors::error register_registry_actor() {
  return caors::reflection::class_<registry_actor>("caors.test.reflection.registry_actor",
                                                   registry_actor_guid)
      .field("id", &registry_actor::id)
      .field("health", &registry_actor::health)
      .commit();
}

[[nodiscard]] caors::error register_view_actor() {
  return caors::reflection::class_<view_actor>("caors.test.reflection.view_actor", view_actor_guid)
      .field("id", &view_actor::id)
      .field("health", &view_actor::health)
      .commit();
}

} // namespace

TEST_CASE("reflection - registry registers and finds types", "[reflection]") {
  REQUIRE(register_registry_actor() == caors::error{});

  CHECK(caors::reflection::size() >= 1);

  auto by_name = caors::reflection::find("caors.test.reflection.registry_actor");
  REQUIRE(by_name);
  CHECK(caors::reflection::contains(by_name.id()));
  CHECK(by_name.field_count() == 2);

  auto by_guid = caors::reflection::find(registry_actor_guid);
  REQUIRE(by_guid);
  CHECK(by_guid.id() == by_name.id());
}

TEST_CASE("reflection - class builder can attach to explicit meta type", "[reflection]") {
  const caors::meta::type type =
      caors::meta::type_<explicit_meta_actor>("caors.test.reflection.explicit_meta_actor").get();
  REQUIRE(type.valid());
  CHECK(caors::meta::find<explicit_meta_actor>().id() == type.id());

  const caors::error status = caors::reflection::class_<explicit_meta_actor>(type)
                                  .field("id", &explicit_meta_actor::id)
                                  .commit();

  REQUIRE(
      (status == caors::error{} || status == reflection_error(caors::errc::duplicate_reflection)));

  auto reflected = caors::reflection::find(type.id());
  REQUIRE(reflected);
  CHECK(reflected.field_count() == 1);
  CHECK(reflected.find_field("id"));
}

TEST_CASE("reflection - class builder reports invalid explicit meta type", "[reflection]") {
  const caors::error status = caors::reflection::class_<explicit_meta_actor>(caors::meta::type{})
                                  .field("id", &explicit_meta_actor::id)
                                  .commit();

  CHECK(status.failed());
  CHECK(status.domain() == caors::error_domain::reflection);
  CHECK(status.value() == caors::errc::invalid_type);
}

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
  CHECK(health.pointer_depth() == 0);
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

TEST_CASE("reflection - views remain valid for append-only registry", "[reflection]") {
  const caors::error register_status = register_view_actor();
  REQUIRE((register_status == caors::error{} ||
           register_status == reflection_error(caors::errc::duplicate_reflection)));

  auto reflected = caors::reflection::find("caors.test.reflection.view_actor");
  REQUIRE(reflected);

  auto field = reflected.find_field("id");
  REQUIRE(field);
  CHECK(reflected.valid());
  CHECK(field.valid());

  CHECK(register_view_actor() == reflection_error(caors::errc::duplicate_reflection));

  CHECK(reflected.valid());
  CHECK(field.valid());
}

TEST_CASE("reflection - registry registers child type with parent", "[reflection]") {
  REQUIRE(caors::reflection::class_<parent_actor>("caors.test.reflection.parent_actor")
              .field("id", &parent_actor::id)
              .commit() == caors::error{});
  const auto reflected_actor = caors::reflection::find("caors.test.reflection.parent_actor");
  REQUIRE(reflected_actor);

  REQUIRE(caors::reflection::class_<child_player>("caors.test.reflection.child_player",
                                                  reflected_actor.id())
              .field("level", &child_player::level)
              .commit() == caors::error{});

  auto reflected_player = caors::reflection::find("caors.test.reflection.child_player");
  REQUIRE(reflected_player);
  CHECK(reflected_player.info().parent_id() == reflected_actor.id());
}

TEST_CASE("reflection - registry reports duplicate reflection", "[reflection]") {
  REQUIRE(caors::reflection::class_<duplicate_actor>("caors.test.reflection.duplicate_actor")
              .field("id", &duplicate_actor::id)
              .commit() == caors::error{});
  CHECK(caors::reflection::class_<duplicate_actor>("caors.test.reflection.duplicate_actor")
            .field("id", &duplicate_actor::id)
            .commit() == reflection_error(caors::errc::duplicate_reflection));
}

TEST_CASE("reflection - registry validates duplicate fields", "[reflection]") {
  CHECK(caors::reflection::class_<duplicate_field_owner>("caors.test.reflection.duplicate_field")
            .field("id", &duplicate_field_owner::id)
            .field("id", &duplicate_field_owner::health)
            .commit() == reflection_error(caors::errc::duplicate_field));
}
